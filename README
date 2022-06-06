
# About

This repository holds a fork of Tor's 0.4.5.7 codebase aimed to experiment on
Flexible Anonymous Networks (FANs). FAN is a novel methodology to develop and
maintain an anonymous network. It builds upon a new execution model to address
risks inherent to protocol robustness, and limitations from protocol
negotiations.  

This repository holds a set of plugins that can be hooked within different
parts of the Tor codebase. These plugins can be found in directories located at
`src/plugins/`. Each plugin can be independently built from the Tor binary by
invoking `make` in the plugin directory, assuming you have installed clang-6.0.
Invoking `make` would compile the plugin to eBPF and store it in a '.o' file.

A set of `.o` object files with a `.plugin` that describes where those pieces of
code must be hooked make up together a `plugin`.

The JIT compiler of eBPF and interpreter are located in src/ubpf, compiled and
statically linked to Tor when Tor builds.

You'll find .c/.h files at src/core/or/plugin* that manage plugins and give an
interface for loading, compiling and calling them.

You may find an example of a declared hook in src/core/or/relay.c#L2022.

# Building Tor

To build Tor from source:
        ./configure && make && make install

To build Tor from a just-cloned git repository:
        sh autogen.sh && ./configure && make && make install


# Loading Plugins at boot

Tor will automatically load and compile plugins at boot if the Tor config file
(torrc) contains:  

```
EnablePlugins 1
PluginsDirectory path/to/plugins/directory
```

# Limitations

This code is experimental and should not, under any circumstance, run over
the real Tor network.  

eBPF plugins: currently plugins cannot directly call another one. We enable
calling each other going through the host, exposing a function that can
bridge independently sandboxed plugins. Functions cannot take more than 5
parameters, and only raw pointers and basic types.

Within one plugin, if multiple functions are declared, all but one must be
inlined.

