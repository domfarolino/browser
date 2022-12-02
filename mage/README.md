# mage

A simple cross-platform[^1] interprocess communication (IPC) library written in
C++. Mage is heavily inspired by Chromium's [Mojo IPC library], and written by
[Dominic Farolino] (a Chromium engineer) in his free time.

Chromium's Mojo is feature-rich and battle-tested, but it has a lot of
Chromium-specific dependencies that prevent it from being portable and usable by
other applications. The motivation for Mage was to create a watered-down version
of Mojo, with no dependencies on the Chromium tree, for learning purposes and
for use in arbitrary C++ applications.

Mage's only dependency is [`//base`], a simple threading a scheduling library
developed alongside Mage, although design work for separating the two entirely
is being considered, to make Mage even more standalone. Mage is also built with
[Bazel] but can be integrated with other toolchains.

<details><summary>History!</summary>

The user-facing IDL portion of mojo was based on Darin Fisher's [ipc_idl]
prototype, which describes a very similar IDL that python generates C++ bindings
from, with the jinja2 templating engine.

In the mid-2010s, development on the "guts" of Mojo began, with a simple
messaging library called [ports]. If you're a Google employee, you can read
[Mojo Core Ports Overview]. Ports now lives inside Mojo in the chromium tree:
https://source.chromium.org/chromium/chromium/src/+/main:mojo/core/ports/.

The Mojo language-specific bindings (like `mojo::Remote`, etc.) are built on top
of `mojo/core` which is in turn built on top of ports.

Mojo is built in an impressively layered fashion, allowing for its internals
(much of the stuff underneath the bindings layer) to be swapped out for an
entirely different backing implementation with no (Chromium) developer
consequences. Ken Rockot built the [ipcz] library to experiment replacing much
of the Mojo internals with his IPC system that implements message passing with
shared memory pools instead of explicit message passing with sockets and file
descriptors, etc.
</details>

## Overview

Mage IPC allows you to seamlessly send asynchronous messages to an object that
lives in another process, thread, or even the same thread, without the sender
having to know anything about where the target object actually is.

To use Mage, you need to be familiar with three concepts from its public API:
 - `mage::MessagePipe`
 - `mage::Remote`
 - `mage::Receiver`

Each end of a message pipe is represented by a `MessagePipe`, which can be
passed across processes. Ultimately, a `MessagePipe` representing one end of a
message pipe will get bound to a `Remote`, and the other end's `MessagePipe`
will get bound to a `Receiver`. It is through these objects that arbitrary
messages get passed as IPCs.

### `mage::Remote`

Once bound, a `Remote<magen::Foo>` represents a local "proxy" for a `magen::Foo`
object that may be implemented in another process. You can synchronously invoke
any of `magen::Foo` interface's methods on a `Remote<magen::Foo>`, and the
remote proxy will forward the message to the right place, wherever the target
object actually lives (even if it is moving around). See the next section for
defining interfaces in Magen IDL.

```cpp
MessagePipe remote_pipe = /* get pipe from somewhere */;
mage::Remote<magen::Foo> remote(remote_pipe);

// Start sending IPCs!
remote->ArbitraryMessage("some payload");
```

### `mage::Receiver`

Messages sent over a bound `mage::Remote<magen::Foo>` get queued on the other
end of the pipe's `MessagePipe`, until _it_ is bound to a corresponding
`mage::Receiver<magen::Foo>`. A `Receiver<magen::Foo>` represents the concrete
implementation of a Mage interface `magen::Foo`. The receiver itself does not
handle messages that were dispatched by the corresponding remote, but rather it
has as a reference to a C++ object that implements the `magen::Foo` interface,
and it forwards messages to that user-provided implementation. A receiver for a
Mage interface is typically owned by the implementation of that interface.

Here's what a concrete implementation of a cross-process Mage object looks like:

```cpp
// Instances of this class can receive asynchronous IPCs from other processes.
class FooImpl final : public magen::Foo {
 public:
  Bind(mage::MessagePipe foo_receiver) {
    // Tell `receiver_` that `this` is the concrete implementation of
    // `magen::Foo` that can handle IPCs.
    receiver_.Bind(foo_receiver, this);
  }

  // Implementation of magen::Foo. These methods get invoked by `receiver_` when
  // it reads messages from its corresponding `mage::Remote`.
  void ArbitraryMessage(string) { /* ... */ }
  void AnotherIPC(MessagePipe) { /* ... */ }

 private:
  // The corresponding remote may live in another process.
  mage::Receiver<magen::Foo> receiver_;
};
```


## Magen Interface Definition Language (IDL)

Magen is the [IDL] that describes Mage interfaces. Interfaces are written in
`.magen` files and are understood by the `magen(...)` Bazel rule, which
generates C++ bindings code based on your interface.

The magen IDL is quite simple. Each `.magen` file describes a single interface
with the `interface` keyword, which can have any number of methods described by
their names and parameters.

Single line C-style comments are supported. Here are a list of supported
parameter types:
 - `bool`
 - `int`
 - `long`
 - `double`
 - `char`
 - `string`
 - `MessagePipe`

The types are self-explanatory, with the exception of `MessagePipe`. A
`MessagePipe` that is not bound to a `Remote` or `Receiver` can be passed from
one process, over an existing IPC interface, to be bound to a
`Remote`/`Receiver` in another process. This is the basic primitive with which
it's possible to expand the number of connections that span two processes.

Here's an example of an interface:

```cpp
// This interface is implemented by the parent process. It is used by its child
// processes to communicate commands to the parent process.
interface ParentProcess {
  // Child tells parent process to navigate to `url`, with an arbitrary delay of
  // `delay` seconds.
  NavigateToURL(string url, int delay);

  // ...
  OpenFile(string name, bool truncate);

  // Before calling this, the child will mint two entangled `MessagePipe`s. It
  // binds one to its `ChildProcess` receiver, and passes the other to the
  // parent for use as a remote. The parent binds this to a
  // `mage::Remote<magen::ChildProcess>` so it can send commands back to its
  // child.
  BindChildProcessRemote(MessagePipe child_remote);
}
```


## Using Mage in an application

Using Mage to provide IPC support in an application is pretty simple; there are
only a few steps:

 1. Create your interface in a `.magen` file (covered by the previous section)
 1. Add the `.magen` file to your project's Bazel `BUILD` file
 1. Implement your interface in C++
 1. Use a `Remote` to send IPCs to your cross-process interface (or any thread)

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
├─ .gitignore
```

You have an application (`main.cc`) that takes URLs from user input and fetches
them, but you want to do the fetching in separate process
(`network_process.cc`). Specifically, `main.cc` will spin up the network process
and tell it what URLs to fetch and when.

### 1. Write the Magen interface for the network process

The first thing you need to do is write the Magen interface that `main.cc` will
use to communicate to the network process. This includes a `FetchURL` IPC
message that contains a URL. Magen interfaces are typically defined in a
`magen/ directory`:

```cpp
// network_process/magen/network_process.magen

interface NetworkProcess {
  FetchURL(string url);
}
```

### 2. Add the `.magen` file to a BUILD file

Next, you need to tell your BUILD file about the interface in your `.magen`
file, so it can "build" it. `magen/` directories get their own `BUILD` files
that invoke the Mage build process.

```starlark
# network_process/magen/BUILD

load("//mage/parser:magen_idl.bzl", "magen_idl")

# Generates `network_process.magen.h`, which can be included by depending on the
# ":include" target below.
magen_idl(
  name = "include",
  srcs = [
    "network_process.magen",
  ],
)
```

This tells Mage to generate a C++ header called `network_process.magen.h` based
on the supplied interface. Both `main.cc` and `network_process.cc` can
`#include` this header by listing the `:include` rule as a dependency. For
example, you'd modify `src/BUILD` like so:

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
+    # Allows `main.cc` to `include` the generated interface header.
+    "//network_process/magen:include",
+  ],
  visibility = ["//visibility:public"],
)
```

You'll need to do the same for `//network_process/BUILD`, so that the
`network_process.cc` binary can also `#include` the generated interface code.

### 3. Implement your interface in C++

The C++ object that will _back_ the `magen::NetworkProcess` interface that you
designed will of course live in the `network_process.cc` binary, since that's
where we'll accept URLs from the main process to fetch. We'll need to implement
this interface now:

```cpp
// network_process/network_process.cc

#include "mage/bindings/receiver.h"
#include "network_process/magen/network_process.magen.h" // Generated.

// The network process's concrete implementation of the `magen::NetworkProcess`
// interface. Other processes can talk to us via that interface.
class NetworkProcess final : public magen::NetworkProcess {
 public:
  // Bind a receiver that we get from the parent, so `this` can start receiving
  // cross-process messages.
  NetworkProcess(mage::MessagePipe receiver) {
    receiver_.Bind(receiver, this);
  }

  // magen::NetworkProcess implementation:
  void FetchURL(std::string url) override { /* ... */ }
 private:
  mage::Receiver<magen::NetworkProcess> receiver_;
};

// Network process binary.
int main() {
  // Accept the mage invitation from the process.
  mage::MessagePipe network_receiver = /* ... */;
  NetworkProcess network_process_impl(network_receiver);
  // `network_process_impl` can start receiving asynchronous IPCs from the
  // parent process, directing it to fetch URLs.

  RunApplicationLoopForever();
  return 0;
}
```

### 4. Use a `Remote` to send cross-process IPCs

The main application binary can communicate to the network process with a
`mage::Remote<magen::NetworkProcess>`, by calling the interface's methods.

```cpp
// src/main.cc

#include "mage/bindings/remote.h"
#include "network_process/magen/network_process.magen.h" // Generated.

// Main binary that the user runs.
int main() {
  MessagePipe network_pipe = /* obtained from creating the network process */
  mage::Remote<magen::NetworkProcess> remote(network_pipe);

  remote->FetchURL("https://google.com");
  RunApplicationLoopForever();
  return 0;
}
```


## Sending `MessagePipes` cross-process

The previous section illustrates sending a message over a bound message pipe to
another process, using a single remote/receiver pair that spans the two
processes. But usually you don't want to have a single interface responsible for
every single message that spans two processes. That leads to bad layering and
design. Rather, you often want something like:

```
┌─────────────────────────┐             ┌──────────────────────────┐
│         Proc A          │             │          Proc B          │
│                         │             │                          │
│  ┌───────────────────┐  │             │  ┌────────────────────┐  │
│  │CompositorDirector │  │             │  │CompositorImpl      │  │
│  │                   │  │             │  │                    │  │
│  │   remote──────────┼──┼─────────────┼──┼──►receiver         │  │
│  └───────────────────┘  │             │  └────────────────────┘  │
│                         │             │                          │
│  ┌───────────────────┐  │             │  ┌────────────────────┐  │
│  │RemoteUIService    │  │             │  │UIServiceImpl       │  │
│  │                   │  │             │  │                    │  │
│  │   remote──────────┼──┼─────────────┼──┼──►receiver         │  │
│  └───────────────────┘  │             │  └────────────────────┘  │
│                         │             │                          │
│  ┌───────────────────┐  │             │  ┌────────────────────┐  │
│  │LoggerImpl         │  │             │  │RemoteLogger        │  │
│  │                   │  │             │  │                    │  │
│  │ receiver◄─────────┼──┼─────────────┼──┼──remote            │  │
│  └───────────────────┘  │             │  └────────────────────┘  │
│                         │             │                          │
└─────────────────────────┘             └──────────────────────────┘
```

As long as you have a single interface spanning two processes, you can use it to
indefinitely expand the number of interfaces between them, by passing
`MessagePipe`s over an existing interface:

```cpp
mage::Remote<magen::BoostrapInterface> remote = /* ... */;

// Create two entangled message pipes, and send one (intended for use as a
// receiver) to the remote process.
std::vector<mage::MessagePipe> new_pipes = mage::Core::CreateMessagePipes();

remote->SendCompositorReceiver(new_pipes[1]);
mage::Remote<magen::Compositor> compositor_remote(new_pipes[0]);

// Now you can start invoking methods on the remote compositor, and they'll
// arrive in the remote process once the receiver (new_pipes[1]) gets bound.
compositor_remote->StartCompositing();
```

## Mage invitations

The above sections are great primers on how to use Mage in an existing
application, but they only cover the basics. Every example assumes you already
have a connection between two processes in your system. From there, you can send
messages, and even expand the number of connections that span the two processes.
But **how do you establish the initial connection**? This section introduces the
Mage "invitation" concep, which helps you do this.

TODO: Document this.


## Design limitations

See [docs/design_limitations.md].


## Security considerations

See [docs/security.md].


[^1]: Well, technically it is only Linux & macOS for now 😔. Windows support
will be coming.

[Mojo IPC library]: https://chromium.googlesource.com/chromium/src/+/master/mojo/README.md
[Dominic Farolino]: https://github.com/domfarolino
[`//base`]: https://github.com/domfarolino/browser/tree/master/base
[Bazel]: https://bazel.build/
[ipc_idl]: https://github.com/darinf/ipc_idl
[ports]: https://github.com/darinf/ports
[IDL]: https://en.wikipedia.org/wiki/Interface_description_language
[Mojo Core Ports Overview]: https://docs.google.com/document/d/1PaQEKfHi8pWifyiHRplnYeicjfjRuFJSMe3_zlJzhs8/edit
[ipcz]: https://github.com/krockot/ipcz
[docs/security.md]: docs/security.md
[docs/design_limitations.md]: docs/design_limitations.md
