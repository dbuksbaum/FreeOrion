#include "../FreeOrion/util/AppInterface.h"
#include "../FreeOrion/UI/CUIControls.h"
#include "../FreeOrion/util/MultiplayerCommon.h"
#include "../FreeOrion/util/OptionsDB.h"
#include "../FreeOrion/UI/TechWnd.h"
#include "SDLGGApp.h"
#include "XMLDoc.h"

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

TechTreeWnd* g_tech_tree_wnd = 0;

GG::Wnd* NewCUIButton(const GG::XMLElement& elem)         {return new CUIButton(elem);}
GG::Wnd* NewCUIStateButton(const GG::XMLElement& elem)    {return new CUIStateButton(elem);}
GG::Wnd* NewCUIScroll(const GG::XMLElement& elem)         {return new CUIScroll(elem);}
GG::Wnd* NewCUIScrollTab(const GG::XMLElement& elem)      {return new CUIScroll::ScrollTab(elem);}
GG::Wnd* NewCUIListBox(const GG::XMLElement& elem)        {return new CUIListBox(elem);}
GG::Wnd* NewCUIDropDownList(const GG::XMLElement& elem)   {return new CUIDropDownList(elem);}
GG::Wnd* NewCUIEdit(const GG::XMLElement& elem)           {return new CUIEdit(elem);}
GG::Wnd* NewCUIMultiEdit(const GG::XMLElement& elem)      {return new CUIMultiEdit(elem);}

class TechViewApp : public SDLGGApp
{
public:
    TechViewApp() :
        SDLGGApp(GetOptionsDB().Get<int>("app-width"), 
                 GetOptionsDB().Get<int>("app-height"),
                 false, "tech-view")
    {
        AddWndGenerator("CUIButton", &NewCUIButton);
        AddWndGenerator("CUIStateButton", &NewCUIStateButton);
        AddWndGenerator("CUIScroll", &NewCUIScroll);
        AddWndGenerator("CUIScroll::ScrollTab", &NewCUIScrollTab);
        AddWndGenerator("CUIListBox", &NewCUIListBox);
        AddWndGenerator("CUIDropDownList", &NewCUIDropDownList);
        AddWndGenerator("CUIEdit", &NewCUIEdit);
        AddWndGenerator("CUIMultiEdit", &NewCUIMultiEdit);
        Logger().setPriority(PriorityValue(GetOptionsDB().Get<std::string>("log-level")));
        SetMaxFPS(60.0);
    }

    virtual void Enter2DMode()
    {
        glPushAttrib(GL_ENABLE_BIT | GL_PIXEL_MODE_BIT | GL_TEXTURE_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glViewport(0, 0, AppWidth(), AppHeight()); //removed -1 from AppWidth & Height

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();

        // set up coordinates with origin in upper-left and +x and +y directions right and down, respectively
        // the depth of the viewing volume is only 1 (from 0.0 to 1.0)
        glOrtho(0.0, AppWidth(), AppHeight(), 0.0, 0.0, AppWidth());

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    virtual void Exit2DMode()
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopAttrib();
    }

private:
    virtual void SDLInit()
    {
        const SDL_VideoInfo* vid_info = 0;

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) {
            Logger().errorStream() << "SDL initialization failed: " << SDL_GetError();
            Exit(1);
        }

        SDL_WM_SetCaption("FreeOrion TechView", "FreeOrion TechView");

        if (SDLNet_Init() < 0) {
            Logger().errorStream() << "SDL Net initialization failed: " << SDLNet_GetError();
            Exit(1);
        }

        if (FE_Init() < 0) {
            Logger().errorStream() << "FastEvents initialization failed: " << FE_GetError();
            Exit(1);
        }

        vid_info = SDL_GetVideoInfo();

        if (!vid_info) {
            Logger().errorStream() << "Video info query failed: " << SDL_GetError();
            Exit(1);
        }

        int bpp = boost::lexical_cast<int>(GetOptionsDB().Get<int>("color-depth"));
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        if (24 <= bpp) {
            SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        } else { // assumes 16 bpp minimum
            SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
            SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
            SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
        }

        if (SDL_SetVideoMode(AppWidth(), AppHeight(), bpp, SDL_OPENGL) == 0) {
            Logger().errorStream() << "Video mode set failed: " << SDL_GetError();
            Exit(1);
        }

        if (NET2_Init() < 0) {
            Logger().errorStream() << "SDL Net2 initialization failed: " << NET2_GetError();
            Exit(1);
        }

        SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
        EnableMouseDragRepeat(SDL_DEFAULT_REPEAT_DELAY / 2, SDL_DEFAULT_REPEAT_INTERVAL / 2);

        Logger().debugStream() << "SDLInit() complete.";
        GLInit();
    }

    virtual void GLInit()
    {
        double ratio = AppWidth() / (float)(AppHeight());

        glEnable(GL_BLEND);
        glClearColor(0, 0, 0, 0);
        glViewport(0, 0, AppWidth(), AppHeight());
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(50.0, ratio, 0.0, 10.0);
        gluLookAt(0.0, 0.0, 5.0, 
                  0.0, 0.0, 0.0, 
                  0.0, 1.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        SDL_GL_SwapBuffers();
        Logger().debugStream() << "GLInit() complete.";
    }

    virtual void Initialize()
    {
        m_ui = boost::shared_ptr<ClientUI>(new ClientUI());
        g_tech_tree_wnd = new TechTreeWnd(AppWidth(), AppHeight());
        Register(g_tech_tree_wnd);
        g_tech_tree_wnd->ShowCategory(GetOptionsDB().Get<std::string>("category"));
    }

    virtual void HandleNonGGEvent(const SDL_Event& event)
    {
        switch(event.type) {
        case SDL_QUIT:
            Exit(0);
            return;
        }
    }

    virtual void FinalCleanup() {}

    virtual void SDLQuit()
    {
        FinalCleanup();
        NET2_Quit();
        FE_Quit();
        SDLNet_Quit();
        SDL_Quit();
        Logger().debugStream() << "SDLQuit() complete.";
    }

    boost::shared_ptr<ClientUI> m_ui;
};

// this undoes the line from SDL_main.h that reads "#define main SDL_main"
#ifdef main
#undef main
#endif

extern "C" // use C-linkage, as required by SDL
int main(int argc, char* argv[])
{
    // read and process command-line arguments, if any
    try {
        GetOptionsDB().AddFlag('h', "help", "Print this help message.");
		GetOptionsDB().AddFlag('m', "music-off", "Disables music in the game");
        GetOptionsDB().Add<std::string>("category", "category of techs to show", "ALL");
        GG::XMLDoc doc;
        GetOptionsDB().SetFromCommandLine(argc, argv);
        GetOptionsDB().Set("UI.sound.enabled", false);
        if (GetOptionsDB().Get<bool>("help")) {
            GetOptionsDB().GetUsage(std::cerr);
            return 0;
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "main() caught exception(std::invalid_arg): " << e.what();
        GetOptionsDB().GetUsage(std::cerr);
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "main() caught exception(std::runtime_error): " << e.what();
        GetOptionsDB().GetUsage(std::cerr);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "main() caught exception(std::exception): " << e.what();
        GetOptionsDB().GetUsage(std::cerr);
        return 1;
    } catch (...) {
        std::cerr << "main() caught unknown exception.";
        return 1;
    }

    TechViewApp app;

    try {
        app(); // run app (intialization and main process loop)
    } catch (const std::invalid_argument& e) {
        app.Logger().errorStream() << "main() caught exception(std::invalid_arg): " << e.what();
    } catch (const std::runtime_error& e) {
        app.Logger().errorStream() << "main() caught exception(std::runtime_error): " << e.what();
    } catch (const  boost::io::format_error& e) {
        app.Logger().errorStream() << "main() caught exception(boost::io::format_error): " << e.what();
    } catch (const std::exception& e) {
        app.Logger().errorStream() << "main() caught exception(std::exception): " << e.what();
    }
    return 0;
}
