#include <Poseidon/UI/Controls/IosBootModalSpike.hpp>

#if defined(POSEIDON_TARGET_IOS)

#import <UIKit/UIKit.h>
#include <chrono>
#include <cstdio>

namespace Poseidon
{

namespace
{

// Same lookup IosKeyboardAccessory.mm uses for the *existing* key window;
// here we want the connected UIWindowScene itself, to attach a *new* window
// to it via the modern initWithWindowScene: initializer (SDL's own launch
// window is built the same way -- see SDL_uikitappdelegate.m).
UIWindowScene* ActiveWindowScene()
{
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
    {
        if ([scene isKindOfClass:UIWindowScene.class])
        {
            return (UIWindowScene*)scene;
        }
    }
    return nil;
}

// Pump the run loop for a short burst without blocking on any particular
// condition -- used to let a just-created window/view hierarchy complete its
// first layout+display pass before we ask it to present anything on top.
void PumpRunLoopBriefly(double seconds)
{
    const NSTimeInterval deadline = [NSDate timeIntervalSinceReferenceDate] + seconds;
    while ([NSDate timeIntervalSinceReferenceDate] < deadline)
    {
        @autoreleasepool
        {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
        }
    }
}

} // namespace

void RunIosBootModalSpike()
{
    @autoreleasepool
    {
        const auto start = std::chrono::steady_clock::now();

        // Our own window/level, clearly above SDL's launch-screen window
        // (which uses UIWindowLevelNormal + 1.0 -- see SDL_uikitappdelegate.m)
        // so same-level ordering ambiguity can't hide us behind it.
        UIWindowScene* scene = ActiveWindowScene();
        UIWindow* window = scene != nil ? [[UIWindow alloc] initWithWindowScene:scene]
                                         : [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
        window.windowLevel = UIWindowLevelNormal + 2.0;
        UIViewController* rootVC = [[UIViewController alloc] init];
        rootVC.view.backgroundColor = [UIColor blackColor];
        window.rootViewController = rootVC;
        [window makeKeyAndVisible];

        std::fprintf(stdout, "[IosBootModalSpike] window made key+visible (scene=%s), letting it settle\n",
                     scene != nil ? "yes" : "no(fallback)");
        std::fflush(stdout);

        // Give the window/view hierarchy one real layout+display pass before
        // presenting anything on top of it -- presenting in the same run-loop
        // turn as makeKeyAndVisible can silently no-op since the presenting
        // view controller's view hasn't appeared yet.
        PumpRunLoopBriefly(0.2);

        __block bool dismissed = false;
        UIAlertController* alert =
            [UIAlertController alertControllerWithTitle:@"M0 Boot Modal Spike"
                                                 message:@"Tap OK to continue booting."
                                          preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                                   style:UIAlertActionStyleDefault
                                                 handler:^(UIAlertAction* _Nonnull action) {
                                                     (void)action;
                                                     dismissed = true;
                                                 }]];
        [rootVC presentViewController:alert
                              animated:YES
                            completion:^{
                                std::fprintf(stdout, "[IosBootModalSpike] presentation completion fired\n");
                                std::fflush(stdout);
                            }];

        std::fprintf(stdout, "[IosBootModalSpike] presented, pumping run loop until dismissed\n");
        std::fflush(stdout);

        // Nested run-loop pump: main() is running synchronously inside
        // SDL_CallMainFunction's call stack (itself inside the UIKit run
        // loop's postFinishLaunch), so we must NOT spin/sleep here -- turning
        // the run loop this way is what lets the alert's button tap actually
        // get delivered and processed.
        while (!dismissed)
        {
            @autoreleasepool
            {
                [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                          beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
            }
        }

        window.hidden = YES;
        window = nil;

        const double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::fprintf(stdout, "[IosBootModalSpike] dismissed after %.2fs, resuming boot\n", elapsedSeconds);
        std::fflush(stdout);
    }
}

} // namespace Poseidon

#endif
