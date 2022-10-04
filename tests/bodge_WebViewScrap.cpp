#include "../gui/choc_WebView.h"
#include "../gui/choc_DesktopWindow.h"

#include "choc_UnitTest.h"

#include <chrono>

namespace bodge
{

void testWebView (choc::test::TestProgress& progress)
{
    static size_t guiHelperId = 0;
    CHOC_CATEGORY (WebView);

    struct GuiTestHelper
    {
        GuiTestHelper (uint32_t timeoutMs) : window ({}), testTimeoutMs (timeoutMs)
        {
            ++guiHelperId;
            std::cout << guiHelperId << ": GuiTestHelper::GuiTestHelper()\n";
            window.setResizable (true); // makes the window closable on macOS
            std::cout << guiHelperId << ": GuiTestHelper::GuiTestHelper(): made window resizable\n";
            window.windowClosed = [] { choc::messageloop::stop(); };
        }

        ~GuiTestHelper()
        {
            std::cout << guiHelperId << ": GuiTestHelper::~GuiTestHelper()\n";
        }

        void signalClose()
        {
            std::cout << guiHelperId << ": GuiTestHelper::signalClose()\n";
            choc::messageloop::postMessage ([this] { close(); });
        }

        bool run (void* viewHandle)
        {
            window.setContent (viewHandle);
            std::cout << guiHelperId << ": GuiTestHelper::run(): about to create timer\n";
            startTime = std::chrono::high_resolution_clock::now();
            // bodge: workaround timers being fired early, check every second, we'll do a duration
            // check inside the callback
            timeout = choc::messageloop::Timer (1000, [this] { return onTimeout(); });
            std::cout << guiHelperId << ": GuiTestHelper::run(): created timer. about to run messageloop.\n";
            choc::messageloop::run();

            return ! timedOut;
        }

        choc::ui::DesktopWindow window;

    private:
        void close()
        {
            std::cout << guiHelperId << ": GuiTestHelper::close(): kill timeout\n";
            timeout = {};
            const auto* handle = window.getWindowHandle();
           #if CHOC_LINUX
            gtk_window_close ((GtkWindow*) handle);
           #elif CHOC_WINDOWS
            SendMessage ((HWND) handle, WM_CLOSE, 0, 0);
           #elif CHOC_APPLE
            choc::objc::call<void> ((id) handle, "performClose:", nullptr);
           #endif
        }

        bool onTimeout()
        {
            const auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::high_resolution_clock::now() - startTime);
            if (duration >= static_cast<std::chrono::milliseconds> (testTimeoutMs))
            {
                std::cout << guiHelperId << ": GuiTestHelper::onTimeout(): " << duration.count() << " elapsed\n";
                timedOut = true;
                signalClose();
            }

            return ! false;
        }

        bool timedOut = false;
        uint32_t testTimeoutMs = 0;
        choc::messageloop::Timer timeout;
        using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
        TimePoint startTime;
        // auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(foo - now);

    };

    struct WebViewTestHelper
    {
        using FetchResource = choc::ui::WebView::Options::FetchResource;
        WebViewTestHelper (FetchResource fetchResource, uint32_t timeoutMs)
            : guiHelper (timeoutMs), webview ({ false, fetchResource })
        {
            webview.bind ("signalTestFinished", [this](const choc::value::ValueView& args) -> choc::value::Value
            {
                testResultFromJavascript = choc::json::toString (args[0], false);
                guiHelper.signalClose();

                return {};
            });
        }

        bool run()
        {
            return guiHelper.run (webview.getViewHandle());
        }

        GuiTestHelper guiHelper;
        choc::ui::WebView webview;

        std::string testResultFromJavascript;
    };

    constexpr uint32_t timeoutMs = 30 * 1000;

    {
        CHOC_TEST (InitialNavigationToRootUri)

        std::cout << "debug InitialNavigationToRootUri: about to construct GuiTestHelper\n";
        GuiTestHelper helper (timeoutMs);
        std::cout << "debug InitialNavigationToRootUri: constructed GuiTestHelper\n";

        std::string routeNavigatedTo;
        const auto fetchResource = [&](const auto& path) -> choc::ui::WebView::Options::Resource
        {
            std::cout << "debug InitialNavigationToRootUri: did signal to close\n";
            routeNavigatedTo = path;
            helper.signalClose();

            return {};
        };

        std::cout << "debug InitialNavigationToRootUri: about to construct webview\n";
        choc::ui::WebView webview ({ false, fetchResource });
        std::cout << "debug InitialNavigationToRootUri: constructed webview\n";

        CHOC_EXPECT_TRUE (helper.run (webview.getViewHandle()));
        CHOC_EXPECT_EQ (routeNavigatedTo, "/");
    }

    // {
    //     CHOC_TEST (SuccessfulResponseContains200StatusCode)

    //     const auto fetchResource = [](const auto& path) -> choc::ui::WebView::Options::Resource
    //     {
    //         if (path == "/")
    //         {
    //             static constexpr const auto html = R"xxx(
    //             <!DOCTYPE html>
    //             <html>
    //                 <head> <title>Page Title</title> </head>
    //                 <body>
    //                 <script>
    //                     async function runTest() {
    //                         try {
    //                             const response = await fetch("./test");
    //                             const {ok, status, statusText} = response;
    //                             const json = await response.json();
    //                             signalTestFinished({text: json.text, ok, status, statusText});
    //                         } catch (e) {
    //                             signalTestFinished({errorMessage: e.message});
    //                         }
    //                     }
    //                     (async () => {
    //                         await runTest();
    //                     })();
    //                 </script>
    //                 </body>
    //             </html>
    //             )xxx";

    //             return
    //             {
    //                 std::vector<uint8_t> (reinterpret_cast<const uint8_t*> (html), reinterpret_cast<const uint8_t*> (html) + strlen (html)),
    //                 "text/html"
    //             };
    //         }

    //         if (path == "/test")
    //         {
    //             static constexpr const auto json = R"({"text": "it worked!"})";

    //             return
    //             {
    //                 std::vector<uint8_t> (reinterpret_cast<const uint8_t*> (json), reinterpret_cast<const uint8_t*> (json) + strlen (json)),
    //                 "application/json"
    //             };
    //         }

    //         return {};
    //     };

    //     WebViewTestHelper helper (fetchResource, timeoutMs);

    //     CHOC_EXPECT_TRUE (helper.run());
    //     const auto expected = choc::value::createObject ("expected",
    //                                                      "text", "it worked!",
    //                                                      "ok", true,
    //                                                      "status", 200,
    //                                                      "statusText", "OK");
    //     CHOC_EXPECT_EQ (helper.testResultFromJavascript, choc::json::toString (expected, false));
    // }

    // {
    //     CHOC_TEST (EmptyOptionalResourceRespondsWith404StatusCode)

    //     const auto fetchResource = [](const auto& path) -> std::optional<choc::ui::WebView::Options::Resource>
    //     {
    //         if (path == "/")
    //         {
    //             static constexpr const auto html = R"xxx(
    //             <!DOCTYPE html>
    //             <html>
    //                 <head> <title>Page Title</title> </head>
    //                 <body>
    //                 <script>
    //                     async function runTest() {
    //                         try {
    //                             const response = await fetch("./test");
    //                             const {ok, status, statusText} = response;
    //                             signalTestFinished({ok, status, statusText});
    //                         } catch (e) {
    //                             signalTestFinished({errorMessage: e.message});
    //                         }
    //                     }
    //                     (async () => {
    //                         await runTest();
    //                     })();
    //                 </script>
    //                 </body>
    //             </html>
    //             )xxx";

    //             return choc::ui::WebView::Options::Resource
    //             {
    //                 std::vector<uint8_t> (reinterpret_cast<const uint8_t*> (html), reinterpret_cast<const uint8_t*> (html) + strlen (html)),
    //                 "text/html"
    //             };
    //         }

    //         return {};
    //     };

    //     WebViewTestHelper helper (fetchResource, timeoutMs);

    //     CHOC_EXPECT_TRUE (helper.run());
    //     const auto expected = choc::value::createObject ("expected",
    //                                                      "ok", false,
    //                                                      "status", 404,
    //                                                      "statusText", "Not Found");
    //     CHOC_EXPECT_EQ (helper.testResultFromJavascript, choc::json::toString (expected, false));
    // }

    // {
    //     CHOC_TEST (ExceptionWhilstFetchingCausesNetworkError)

    //     const auto fetchResource = [](const auto& path) -> choc::ui::WebView::Options::Resource
    //     {
    //         if (path == "/")
    //         {
    //             static constexpr const auto html = R"xxx(
    //             <!DOCTYPE html>
    //             <html>
    //                 <head> <title>Page Title</title> </head>
    //                 <body></body>
    //                 <script>
    //                     async function runTest() {
    //                         try {
    //                             const response = await fetch("./error.json");
    //                         } catch (e) {
    //                             signalTestFinished({exceptionType: e.name});
    //                         }
    //                     }
    //                     (async () => {
    //                         await runTest();
    //                     })();
    //                 </script>
    //             </html>
    //             )xxx";

    //             return
    //             {
    //                 std::vector<uint8_t> (reinterpret_cast<const uint8_t*> (html), reinterpret_cast<const uint8_t*> (html) + strlen (html)),
    //                 "text/html"
    //             };
    //         }

    //         throw "cause `fetch` to throw";
    //     };

    //     WebViewTestHelper helper (fetchResource, timeoutMs);

    //     CHOC_EXPECT_TRUE (helper.run());
    //     const auto expected = choc::value::createObject ("expected",
    //                                                      "exceptionType", "TypeError");
    //     CHOC_EXPECT_EQ (helper.testResultFromJavascript, choc::json::toString (expected, false));
    // }

    // {
    //     CHOC_TEST (CanFetchJavascriptModule)

    //     const auto fetchResource = [](const auto& path) -> std::optional<choc::ui::WebView::Options::Resource>
    //     {
    //         if (path == "/")
    //         {
    //             static constexpr const auto html = R"xxx(
    //             <!DOCTYPE html>
    //             <html>
    //                 <head> <title>Page Title</title> </head>
    //                 <body>
    //                 <script type="module">
    //                     import { runTest } from "./test.js";

    //                     (async () => {
    //                         await runTest();
    //                     })();
    //                 </script>
    //                 </body>
    //             </html>
    //             )xxx";

    //             return choc::ui::WebView::Options::Resource
    //             {
    //                 std::vector<uint8_t> (reinterpret_cast<const uint8_t*> (html), reinterpret_cast<const uint8_t*> (html) + strlen (html)),
    //                 "text/html"
    //             };
    //         }

    //         if (path == "/test.js")
    //         {
    //             const auto js = R"(
    //                 export async function runTest() {
    //                     signalTestFinished({text: "module"});
    //                 }
    //             )";

    //             return choc::ui::WebView::Options::Resource
    //             {
    //                 std::vector<uint8_t> (reinterpret_cast<const uint8_t*> (js), reinterpret_cast<const uint8_t*> (js) + strlen (js)),
    //                 "application/javascript"
    //             };
    //         }

    //         return {};
    //     };

    //     WebViewTestHelper helper (fetchResource, timeoutMs);

    //     CHOC_EXPECT_TRUE (helper.run());
    //     const auto expected = choc::value::createObject ("expected",
    //                                                      "text", "module");
    //     CHOC_EXPECT_EQ (helper.testResultFromJavascript, choc::json::toString (expected, false));
    // }

    // {
    //     CHOC_TEST (CanFetchImgTag)

    //     const auto fetchResource = [](const auto& path) -> choc::ui::WebView::Options::Resource
    //     {
    //         if (path == "/")
    //         {
    //             static constexpr const auto html = R"xxx(
    //             <!DOCTYPE html>
    //             <html>
    //                 <head> <title>Page Title</title> </head>
    //                 <body>
    //                 <img id="red" src="./red.bmp">
    //                 <script>
    //                     async function runTest() {
    //                         const img = document.getElementById("red");
    //                         img.addEventListener("load", () => {
    //                             const canvas = document.createElement("canvas");
    //                             const width = img.width;
    //                             const height = img.height;
    //                             canvas.width = width;
    //                             canvas.height = height;

    //                             const context = canvas.getContext("2d");
    //                             context.drawImage(img, 0, 0, width, height);
    //                             const pixels = context.getImageData(0, 0, width, height);
    //                             signalTestFinished({pixelValue: pixels.data[0]});
    //                         });
    //                     }

    //                     (async () => {
    //                         await runTest();
    //                     })();
    //                 </script>
    //                 </body>
    //             </html>
    //             )xxx";

    //             return
    //             {
    //                 std::vector<uint8_t> (reinterpret_cast<const uint8_t*> (html), reinterpret_cast<const uint8_t*> (html) + strlen (html)),
    //                 "text/html"
    //             };
    //         }

    //         if (path == "/red.bmp")
    //         {
    //             static constexpr const uint8_t bitmap[] = {
    //                 66, 77, 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 0
    //             };

    //             return
    //             {
    //                 std::vector<uint8_t> (std::addressof (bitmap[0]), std::addressof (bitmap[0]) + (sizeof (bitmap) / sizeof (bitmap[0]))),
    //                 "image/bmp"
    //             };
    //         }

    //         return {};
    //     };

    //     WebViewTestHelper helper (fetchResource, timeoutMs);

    //     CHOC_EXPECT_TRUE (helper.run());
    //     const auto expected = choc::value::createObject ("expected",
    //                                                      "pixelValue", 255);
    //     CHOC_EXPECT_EQ (helper.testResultFromJavascript, choc::json::toString (expected, false));
    // }
}

bool runAllTests (choc::test::TestProgress& progress)
{
    testWebView (progress);

    progress.printReport();
    return progress.numFails == 0;
}

}

//==============================================================================
int main()
{
    constexpr size_t maxRuns = 5000;

    for (size_t i = 0; i < maxRuns; ++i)
    {
        std::cout << "run " << i + 1 << "/" << maxRuns << "\n";

        if (i == 375)
        {
            std::cout << "will it hang on windows here again?\n";
        }

        choc::test::TestProgress progress;
        if (! bodge::runAllTests (progress))
            return 1;
    }

    std::cout << "didn't fail in " << maxRuns << " runs\n";

    return 0;

    // choc::test::TestProgress progress;
    // return bodge::runAllTests (progress) ? 0 : 1;
}
