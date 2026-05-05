// Composition root: builds the dependency graph from the bottom up and hands
// it to the wxWidgets UI layer. This is the only place where concrete adapter
// types are mentioned - everything downstream depends on interfaces.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/games/magic/MagicCardPreviewSource.hpp"
#include "ccm/games/magic/MagicGameModule.hpp"
#include "ccm/games/pokemon/PokemonGameModule.hpp"
#include "ccm/infra/CprHttpClient.hpp"
#include "ccm/infra/JsonCollectionRepository.hpp"
#include "ccm/infra/JsonSetRepository.hpp"
#include "ccm/infra/LocalImageStore.hpp"
#include "ccm/infra/StdFileSystem.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/CollectionService.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/SetService.hpp"
#include "ccm/ui/AppContext.hpp"
#include "ccm/ui/MainFrame.hpp"

#include <wx/app.h>
#include <wx/icon.h>
#include <wx/image.h>
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {

// All Game::X -> directory string mappings live in one place. Eliminates the
// need for the repositories to know about concrete game module classes.
std::string dirNameForGame(ccm::Game g) {
    switch (g) {
        case ccm::Game::Magic:   return "magic";
        case ccm::Game::Pokemon: return "pokemon";
    }
    return "magic";
}

}  // namespace

class CcmApp : public wxApp {
public:
    bool OnInit() override {
        // wxImage knows about PNG/JPEG once these handlers are registered.
        wxImage::AddHandler(new wxPNGHandler);
        wxImage::AddHandler(new wxJPEGHandler);

        // --- locate config + data directory next to the executable -------
        const std::filesystem::path exeDir =
            std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdString())
                .parent_path();
        const auto configPath = exeDir / "config.json";

        // --- build the dependency graph -----------------------------------
        fs_       = std::make_unique<ccm::StdFileSystem>();
        config_   = std::make_unique<ccm::ConfigService>(*fs_, configPath, exeDir);

        if (auto init = config_->initialize(); !init) {
            wxMessageBox("Failed to load configuration: " + init.error(),
                         "Startup error", wxOK | wxICON_ERROR);
            return false;
        }

        http_     = std::make_unique<ccm::CprHttpClient>();
        magicMod_ = std::make_unique<ccm::MagicGameModule>(*http_);
        pokeMod_  = std::make_unique<ccm::PokemonGameModule>(*http_);

        magicRepo_ = std::make_unique<ccm::JsonCollectionRepository<ccm::MagicCard>>(
            *fs_, *config_, &dirNameForGame);
        setRepo_   = std::make_unique<ccm::JsonSetRepository>(*fs_, *config_, &dirNameForGame);
        imgStore_  = std::make_unique<ccm::LocalImageStore>(*fs_, *config_, &dirNameForGame);

        imgSvc_       = std::make_unique<ccm::ImageService>(*imgStore_);
        magicCollSvc_ = std::make_unique<ccm::CollectionService<ccm::MagicCard>>(
            *magicRepo_, *imgStore_);
        setSvc_       = std::make_unique<ccm::SetService>(*setRepo_);
        setSvc_->registerModule(magicMod_.get());
        setSvc_->registerModule(pokeMod_.get());

        magicPrevSrc_ = std::make_unique<ccm::MagicCardPreviewSource>(*http_);
        previewSvc_   = std::make_unique<ccm::CardPreviewService>(*http_);
        previewSvc_->registerSource(ccm::Game::Magic, *magicPrevSrc_);

        ctx_ = std::make_unique<ccm::ui::AppContext>(ccm::ui::AppContext{
            *config_,
            *magicCollSvc_,
            *setSvc_,
            *imgSvc_,
            *previewSvc_,
            *magicMod_,
            *pokeMod_,
        });

        auto* frame = new ccm::ui::MainFrame(*ctx_);
#ifdef __WXMSW__
        frame->SetIcon(wxICON(ccm_main_icon));
#endif
        frame->Show(true);
        return true;
    }

private:
    // Order matters - destruction is reverse, so put services that depend on
    // others *after* their deps in the member list.
    std::unique_ptr<ccm::StdFileSystem>                            fs_;
    std::unique_ptr<ccm::ConfigService>                            config_;
    std::unique_ptr<ccm::CprHttpClient>                            http_;
    std::unique_ptr<ccm::MagicGameModule>                          magicMod_;
    std::unique_ptr<ccm::PokemonGameModule>                        pokeMod_;
    std::unique_ptr<ccm::JsonCollectionRepository<ccm::MagicCard>> magicRepo_;
    std::unique_ptr<ccm::JsonSetRepository>                        setRepo_;
    std::unique_ptr<ccm::LocalImageStore>                          imgStore_;
    std::unique_ptr<ccm::ImageService>                             imgSvc_;
    std::unique_ptr<ccm::CollectionService<ccm::MagicCard>>        magicCollSvc_;
    std::unique_ptr<ccm::SetService>                               setSvc_;
    std::unique_ptr<ccm::MagicCardPreviewSource>                   magicPrevSrc_;
    std::unique_ptr<ccm::CardPreviewService>                       previewSvc_;
    std::unique_ptr<ccm::ui::AppContext>                           ctx_;
};

wxIMPLEMENT_APP(CcmApp);
