# OSDI implementation for NGSPICE

OSDI (Open Source Device Interface) is a simulator independent device interface, that is used by the OpenVAF compiler.
Implementing this interface in NGSPICE allows loading Verilog-A models compiled by OpenVAF.
The interface is fixed and does not require the compiler to know about NGSPICE during compilation.
NGSPICE also doesn't need to know anything about the compiled models at compilation.
Therefore, these models can be loaded dynamically at runtime.

To that end the `osdi` command is provided.
It allows loading a dynamic library conforming to OSDI.
Example usage: `osdi diode.osdi`.

If used within a netlist the command requires the `pre_` prefix.
This ensures that the devices are loaded before the netlist is parsed.

Example usage: `pre_osdi diode.osdi`

If a relative path is provided to the `osdi` command in a netlist, it will resolve that path **relative to the netlist**, not relative to current working directory.
This ensures that netlists can be simulated from any directory

## Selecting modules by model level

The `osdi_bind` command lets a `.model` card select an OSDI module by its `level`.
Example usage: `osdi_bind pmos 72 bsimcmg_va type -1 version 111`

This makes `pmos` cards at level 72 use the module `bsimcmg_va` with `type=-1`.
The parameter and value, and the version, are optional.
When several modules are bound to one level, a card selects the one whose version starts the card's `version`, or else the last bound module that is loaded.

`spinit` holds the default bindings, which `.spiceinit` or `pre_osdi_bind` can override.
The module must still be loaded with `osdi` or `pre_osdi`.

## Build Instructions

To compile NGSPICE with OSDI support ensure that the `--enable-predictor` and `--enable-osdi` flags are used.
The `compile_linux.sh` file enables these flags by default.



