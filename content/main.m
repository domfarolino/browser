#import <Cocoa/Cocoa.h>

int main(int argc, const char * argv[]) {
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  id applicationName = [[NSProcessInfo processInfo] processName];
  id window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 600, 600)
                                          styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
  [window cascadeTopLeftFromPoint:NSMakePoint(20,20)];
  [window setTitle: applicationName];
  [window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
  [NSApp run];
  return 0;
}
