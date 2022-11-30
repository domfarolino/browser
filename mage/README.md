# mage

A simple cross-platform (well, only Linux & macOS for now 😔) interprocess
communication IPC) library written in C++. Mage is heavily inspired by
Chromium's [Mojo IPC library], and written by [Dominic Farolino] (a Chromium
engineer) in his free time.

Chromium's Mojo is feature-rich and battle-tested, but it has a lot of
Chromium-specific dependencies that prevent it from being portable and usable by
other applications. The motivation for Mage was to create a watered down version
of Mage, with no dependencies on the Chromium tree, for learning purposes and
for use by arbitrary C++ applications.

Mage's only dependency is [`//base`], a simple threading a scheduling library
developed alongside Mage. Design work for separating the two entirely is being
considered, to make Mage even more standalone.

## Overview

Mage IPC allows you to seamlessly send asynchronous messages to an object that
lives in another process, thread, or even the same thread, without the sender
having to know anything about where the target object actually is.

Public API primitives:
 - `mage::MageHandle`
 - `mage::Remote`
 - `mage::Receiver`

TODO: Finish filling this section out.

## Magen Interface Definition Language (IDL)

Magen is the [IDL] that describes Mage interfaces. Interfaces are written in
`.magen` files and are understood by the `magen_build(...)` Bazel rule, which
generates C++ bindings code based on your interface.

The magen IDL is quite simple. Each `.magen` file describes a single interface
with the `interface` keyword, which can have any number of methods described by
their names and parameters.

C-style comments are supported. Here are a list of supported parameter types:
 - `bool`
 - `int`
 - `long`
 - `double`
 - `char`
 - `string`
 - `MageHandle`

The types are self-explanatory, with the exception of `MageHandle`. A
`MageHandle` that is not bound to a `Remote` or `Receiver` can be passed from
one process, over an existing IPC interface, to be bound to a
`Remote`/`Receiver` in another process. This is the basic primitive with which
it's possible to expand the number of connections that span two processes.

Here's an example of an interface:

```cpp
// This interface is implemented by the master process. It is used by its child
// processes to communicate commands to the master process.
interface ParentProcess {
  // Child tells parent process to navigate to `url`, with an arbitrary delay of
  // `delay` seconds.
  NavigateToURL(string url, int delay);

  // ...
  OpenFile(string name, bool truncate);

  // Before calling this, the child will mint two entangled `MageHandle`s. It
  // binds one to its `ChildProcess` receiver, and passes the other to the
  // parent for use as a remote. The parent binds this to a
  // `mage::Remote<magen::ChildProcess>` so it can send commands back to its
  // child.
  BindChildProcessRemote(MageHandle child_remote);
}
```

## Using Mage in an application

Using Mage to provide IPC support in an application is pretty simple; there are
only a few steps:

 1. Create your interface in a `.magen` file (covered by the previous section)
 1. Reference the `.magen` file in your project's Bazel `BUILD` file
 1. Implement your interface in C++
 1. Wire up a `Remote` and start using your interface cross-process (or
    same-process on any thread)

Let's assume you have a project structure like:

```
my_project/
├─ src/
│  ├─ BUILD
│  ├─ main.cc
├─ network_process/
│  ├─ BUILD
│  ├─ socket.h
│  ├─ network_process.cc
│  ├─ magen/
│  │  ├─ network_process.magen
├─ .gitignore
```

You have a networking project, and you want code inside `main.cc` to tell
`network_process.cc` (a separate process that `main.cc` spins up to take care of
networking tasks) what URL to fetch. In this case, you've already written the
interface through which `main.cc` will communicate with the network process:

```cpp
// network_process/magen/network_process.magen

interface NetworkProcess {
  FetchURL(string url);
}
```

### Reference your `.magen` file in Bazel

Once you've written your `.magen` file you need to tell Bazel about it so it can
"build" it.

```starlark
# network_process/magen/BUILD

load("//mage/parser:magen_build.bzl", "magen_build")
load("@rules_cc//cc:defs.bzl", "cc_library")

magen_build(
  name = "gen",
  srcs = [
    "network_process.magen",
  ],
)
cc_library(
  name = "include",
  hdrs = [":gen"]
)
```

This tells Mage to generate C++ code based on your interface. Both your
`main.cc` and `network_process.cc` have to reference this interface, so they'll
need to `#include` the C++ code that gets generated from this step. That's what
the `cc_library(name="include")` target is for above. With that defined, other
BUILD files can reference the generated code from your interface:

```diff
# src/BUILD

load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
  name = "main",
  srcs = [
    "main.cc",
  ],
+  deps = [
+    "//mage:mage",
+    "//network_process/mage/magen:include",
+  ],
  visibility = ["//visibility:public"],
)
```

You'll need to do the same for `//network_process/BUILD`, so that the
`network_process.cc` binary can also `#include` the generated code from your
interface.

### Implement your interface in C++

The C++ object that will _back_ the `magen::NetworkProcess` interface that you
designed will of course live in the `network_process.cc` binary, since that's
what does the networking, and will accept commands from the main process for
URLs to fetch. We'll need to define this interface now:

```cpp
// network_process/network_process.cc
#include "network_process/magen/network_process.magen.h" // Generated.

// The network process's backing implementation of the `magen::NetworkProcess`
// interface that other processes can use to talk to us!
class NetworkProcess final : public magen::NetworkProcess {
 public:
  // Bind a receiver that we get from the parent, so `this` can start receiving
  // cross-process messages.
  NetworkProcess(mage::MageHandle receiver) {
    receiver_.Bind(receiver, this);
  }

  // magen::NetworkProcess implementation:
  void FetchURL(std::string url) override { /* ... */ }
 private:
  mage::Receiver<magen::NetworkProcess> receiver_;
};

// Network_process binary.
int main() {
  // Accept the mage invitation from the process.
  mage::MageHandle network_receiver = /* ... */
  NetworkProcess network_process_impl(network_receiver);
  // `network_process_impl` can start receiving asynchronous IPCs from the
  // parent process, directing it to fetch URLs.

  RunApplicationLoopForever();
  return 0;
}
```

### Wire up `Remote` and use your cross-process interface

The main application binary can communicate to the network process via a
`mage::Remote<magen::NetworkProcess>`, by the methods defined on the interface.

```cpp
// src/main.cc

#include "network_process/magen/network_process.magen.h" // Generated.
#include "mage/bindings/remote.h"

// Main binary that the user runs.
int main() {
  MageHandle network_remote = /* obtained from creating the network process */
  mage::Remote<magen::NetworkProcess> remote;
  remote.Bind(network_remote);

  remote->FetchURL("https://google.com");

  RunApplicationForever();
  return 0;
}
```

## Security considerations

The process boundary is a critical security boundary for applications, and IPC
implementations play an outsized role in defining it.

The boundary between less-privileged processes and more-privileged ones is
crucial to audit when determining what capabilities or information is shared
across processes. Since this boundary is largely defined by the IPC
implementation being used[^1], IPC libraries tend to care a lot about security.

This is particularly important because less-privileged processes might be
compromised by some sort of application code, so the protection of
more-privileged process depends on it being safe from whatever
possibly-malicious content a less-privileged process might try and throw at it.
Some great documentation about protecting against this can be found in the
Chromium source tree about [compromised renderer processes].

A non-exhaustive list of security considerations that an IPC library might have
are:

 1. A less-privileged process should never be able to terminate a
    more-privileged one
    - Giving control of the lifetime a privileged process to a compromised one
      could result in the corruption of user data, an unusable application, or
      other harmful side effects
 1. The library should validate deserialized data
    - A compromised process may write arbitrary data to a message that breaks
      the data serialization rules in hopes of triggering undefined behavior in
      a more-privileged consumer of the message. The deserialization code in the
      target process is responsible for validating the messages it receives
      from other processes, before handing those messages to user code. When the
      library catches invalid data in a received message, it should attempt to
      kill the process that sent the data, and refuse to pass the message to the
      user code that would normally handle it
 1. IPC libraries often have the concept of a single "master" process that is
    responsible for brokering communication between other processes that don't
    otherwise have the ability to communicate on their own. In Chromium, this is
    literally called [the "Broker" process]. This is important especially in
    sandboxed environments where less-privileged processes can't do many things,
    and require the Broker to grant powerful capabilities
 1. Much more...

As it stands right now, Mage is mostly written as a proof-of-concept, for use in
basic applications, but not audited to uphold the security guarantees that might
be required by major, critical applications in production (including the ones
above). It would be great if Mage could get to this point, and it's certainly
within reach, but right now Mage has not been designed for these purposes.

### Writing safe IPCs

Not all IPC security can be taken care of by an IPC library. Much care needs to
be taken in how interfaces are defined across processes, so that it is not
possible to introduce new, subtle security bugs manually. Here are a list of
good security pointers to keep in mind when writing IPCs, especially those that
communicate across privilege gradients:

 - https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/mojo.md
 - https://www.chromium.org/Home/chromium-security/education/security-tips-for-ipc/
 - https://wiki.mozilla.org/Security/Sandbox/IPCguide
 - https://mozilla.github.io/firefox-browser-architecture/text/0013-ipc-security-models-and-status.html#best-practices

[^1]: Another tool that contributes to defining the process boundary for an
application is whatever sandboxing library is being used, if any. Here are some
examples of open source ones: [openjudge/sandbox], [Chromium sandbox],
[google/sandboxed-api].


[Mojo IPC library]: https://chromium.googlesource.com/chromium/src/+/master/mojo/README.md
[Dominic Farolino]: https://github.com/domfarolino
[`//base`]: https://github.com/domfarolino/browser/tree/master/base
[IDL]: https://en.wikipedia.org/wiki/Interface_description_language
[compromised renderer processes]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/compromised-renderers.md
[the "Broker" process]: https://chromium.googlesource.com/chromium/src/+/master/mojo/core/README.md#:~:text=The%20Broker%20has%20some%20special%20responsibilities
[openjudge/sandbox]: https://github.com/openjudge/sandbox
[Chromium sandbox]: https://chromium.googlesource.com/chromium/src/+/master/docs/design/sandbox.md
[google/sandboxed-api]: https://github.com/google/sandboxed-api
