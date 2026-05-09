// Composition root: builds the dependency graph from the bottom up and hands
// it to the wxWidgets UI layer. This is the only place where concrete adapter
// types are mentioned - everything downstream depends on interfaces.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/games/magic/MagicGameModule.hpp"
#include "ccm/games/pokemon/PokemonGameModule.hpp"
#include "ccm/games/yugioh/YuGiOhGameModule.hpp"
#include "ccm/infra/CprHttpClient.hpp"
#include "ccm/infra/JsonCollectionRepository.hpp"
#include "ccm/infra/JsonSetRepository.hpp"
#include "ccm/infra/LocalImageStore.hpp"
#include "ccm/infra/LocalPreviewByteCache.hpp"
#include "ccm/infra/StdFileSystem.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/CollectionService.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/SetService.hpp"
#include "ccm/ui/AppContext.hpp"
#include "ccm/ui/MagicGameView.hpp"
#include "ccm/ui/MainFrame.hpp"
#include "ccm/ui/PokemonGameView.hpp"
#include "ccm/ui/YuGiOhGameView.hpp"

#include <wx/app.h>
#include <wx/icon.h>
#include <wx/image.h>
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

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
        case ccm::Game::YuGiOh:  return "yugioh";
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

        // --- locate config next to exe, data in user home ----------------
        const std::filesystem::path exeDir =
            std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdString())
                .parent_path();
        const auto configPath = exeDir / "config.json";
        const auto defaultDataDir =
            std::filesystem::path(wxGetHomeDir().ToStdString()) / "ccm3-data";

        // --- build the dependency graph -----------------------------------
        fs_       = std::make_unique<ccm::StdFileSystem>();
        config_   = std::make_unique<ccm::ConfigService>(*fs_, configPath, defaultDataDir);

        if (auto init = config_->initialize(); !init) {
            wxMessageBox("Failed to load configuration: " + init.error(),
                         "Startup error", wxOK | wxICON_ERROR);
            return false;
        }

        http_     = std::make_unique<ccm::CprHttpClient>();
        magicMod_ = std::make_unique<ccm::MagicGameModule>(*http_);
        pokeMod_  = std::make_unique<ccm::PokemonGameModule>(*http_);
        ygoMod_   = std::make_unique<ccm::YuGiOhGameModule>(*http_);

        magicRepo_ = std::make_unique<ccm::JsonCollectionRepository<ccm::MagicCard>>(
            *fs_, *config_, &dirNameForGame);
        pokeRepo_  = std::make_unique<ccm::JsonCollectionRepository<ccm::PokemonCard>>(
            *fs_, *config_, &dirNameForGame);
        ygoRepo_   = std::make_unique<ccm::JsonCollectionRepository<ccm::YuGiOhCard>>(
            *fs_, *config_, &dirNameForGame);
        setRepo_   = std::make_unique<ccm::JsonSetRepository>(*fs_, *config_, &dirNameForGame);
        imgStore_  = std::make_unique<ccm::LocalImageStore>(*fs_, *config_, &dirNameForGame);

        imgSvc_       = std::make_unique<ccm::ImageService>(*imgStore_);
        magicCollSvc_ = std::make_unique<ccm::CollectionService<ccm::MagicCard>>(
            *magicRepo_, *imgStore_);
        pokeCollSvc_  = std::make_unique<ccm::CollectionService<ccm::PokemonCard>>(
            *pokeRepo_, *imgStore_);
        ygoCollSvc_   = std::make_unique<ccm::CollectionService<ccm::YuGiOhCard>>(
            *ygoRepo_, *imgStore_);
        setSvc_       = std::make_unique<ccm::SetService>(*setRepo_);
        setSvc_->registerModule(magicMod_.get());
        setSvc_->registerModule(pokeMod_.get());
        setSvc_->registerModule(ygoMod_.get());

        // Disk-backed preview cache lives next to the executable, in the same
        // location scope as config.json - NOT inside the user's data-storage
        // directory. Rationale: previews are downloaded artifacts, not user
        // data, so they should not move when the user relocates their
        // collection (data-storage path can be reconfigured at runtime), and
        // they should not be uploaded together with the user's collection
        // when the data dir is backed up / synced. The umbrella ".cache/"
        // directory is reserved for any future computed-from-network caches
        // (set-list snapshots, etc.); the leading dot keeps it out of the way
        // for users poking around the install folder. Constructed before
        // previewSvc_ so the service can hold a stable raw pointer to it.
        previewCache_ = std::make_unique<ccm::LocalPreviewByteCache>(
            *fs_,
            exeDir / ".cache" / "preview-cache");
        previewSvc_   = std::make_unique<ccm::CardPreviewService>(*http_, previewCache_.get());
        previewSvc_->registerModule(*magicMod_);
        previewSvc_->registerModule(*pokeMod_);
        previewSvc_->registerModule(*ygoMod_);

        // Per-game UI bundles. Order here is the order shown in the Game menu.
        magicView_ = std::make_unique<ccm::ui::MagicGameView>(
            *config_, *magicCollSvc_, *setSvc_, *imgSvc_, *previewSvc_, *magicMod_);
        pokeView_  = std::make_unique<ccm::ui::PokemonGameView>(
            *config_, *pokeCollSvc_, *setSvc_, *imgSvc_, *previewSvc_, *pokeMod_);
        ygoView_   = std::make_unique<ccm::ui::YuGiOhGameView>(
            *config_, *ygoCollSvc_, *setSvc_, *imgSvc_, *previewSvc_, *ygoMod_);

        ctx_ = std::make_unique<ccm::ui::AppContext>(ccm::ui::AppContext{
            *config_,
            *setSvc_,
            *imgSvc_,
            *previewSvc_,
            *magicMod_,
            *pokeMod_,
            *ygoMod_,
            { magicView_.get(), pokeView_.get(), ygoView_.get() },
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
    // others *after* their deps in the member list. Game views are torn down
    // first so their panels release any references to the typed services.
    std::unique_ptr<ccm::StdFileSystem>                              fs_;
    std::unique_ptr<ccm::ConfigService>                              config_;
    std::unique_ptr<ccm::CprHttpClient>                              http_;
    std::unique_ptr<ccm::MagicGameModule>                            magicMod_;
    std::unique_ptr<ccm::PokemonGameModule>                          pokeMod_;
    std::unique_ptr<ccm::YuGiOhGameModule>                           ygoMod_;
    std::unique_ptr<ccm::JsonCollectionRepository<ccm::MagicCard>>   magicRepo_;
    std::unique_ptr<ccm::JsonCollectionRepository<ccm::PokemonCard>> pokeRepo_;
    std::unique_ptr<ccm::JsonCollectionRepository<ccm::YuGiOhCard>>  ygoRepo_;
    std::unique_ptr<ccm::JsonSetRepository>                          setRepo_;
    std::unique_ptr<ccm::LocalImageStore>                            imgStore_;
    std::unique_ptr<ccm::ImageService>                               imgSvc_;
    std::unique_ptr<ccm::CollectionService<ccm::MagicCard>>          magicCollSvc_;
    std::unique_ptr<ccm::CollectionService<ccm::PokemonCard>>        pokeCollSvc_;
    std::unique_ptr<ccm::CollectionService<ccm::YuGiOhCard>>         ygoCollSvc_;
    std::unique_ptr<ccm::SetService>                                 setSvc_;
    std::unique_ptr<ccm::LocalPreviewByteCache>                      previewCache_;
    std::unique_ptr<ccm::CardPreviewService>                         previewSvc_;
    std::unique_ptr<ccm::ui::MagicGameView>                          magicView_;
    std::unique_ptr<ccm::ui::PokemonGameView>                        pokeView_;
    std::unique_ptr<ccm::ui::YuGiOhGameView>                         ygoView_;
    std::unique_ptr<ccm::ui::AppContext>                             ctx_;
};

wxIMPLEMENT_APP(CcmApp);
