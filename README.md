# Capability / Compartment Mapper

This library provides a mechanism for mapping and analysing CHERI compartments
by studying a capability graph.

The basic premise is simple: if we define a compartment as a graph of
capabilities, then we can stop the world and compile a snapshot of a compartment
by simply traversing the graph, starting with the provided roots.

## Usage

Instrumentation typically requires source changes to the program you want to
study.

For simple uses:

```c++
#include "capmap.h"
```

... then at some interesting point:

```c++
    ...
    capmap::dump_json()
    ...
```

The `Mapper` class provides more control over the scan.

## Implementation limitations

### Work in progress!

Many of the statements in this document are aspirational. Please read this as a
set of intentions, rather than an accurate state of the tools.

### Overheads and performance

Non-trivial capability graphs can be time-consuming to traverse, and can require
a lot of memory (or disk space) to represent. For that reason, data gathering is
configurable, so that you can gather only what you need to gather.

### Included or exlucded memory

By default, memory is scanned as long as it is reachable from at least one
capability (directly or indirectly), and as long as it is mapped at the page
level. The process's memory map is examined for this reason.

It is possible to restrict scans to a more focussed area of memory, for example
to avoid debug infrastructure, or simply to focus on a specific data structure.

In addition, the mapper attempts to exclude its own memory from the scan.

### Concurrent writes

The tool assumes that the memory doesn't change as it is reading it, but takes
no steps to ensure that is the case. In practice, this may be inconsequential,
but if other threads are actively writing in a way that changes the graph, then
the tool will report only one possible view of it. Another observer may see a
different view, depending on the order of accesses.

## Permission Tracking

A capability can only be loaded from some other capability if the latter has
both the Load and the LoadCap permissions simultaneously. Separate Load and
LoadCap capabilities cannot be later combined to allow capabilities to be
loaded. As a result, this tool does not track individual permissions, but rather
_combinations_ of permissions.

Load+LoadCap is always tracked internally, because it is necessary for graph
traversal. Other arbitrary combinations can be tracked as required.

In addition, note that LoadCap is only useful on whole, aligned capability-sized
granules. Similarly, Seal and Unseal are only useful over the range of possible
object types. Bounds outside that range can be ignored. To support this, access
can be evaluated using the whole capability, not just its permissions field.

## Contiguity

For some permissions, such as Load and Store, contiguity may not be seen as
important for security analysis. An attacker may be able to assemble a sequence
of byte-wide loads or stores to piece together the memory contents using a
variety of capabilities, for example.

For others, such as Execute, contiguity is very important, since execution
cannot escape the PCC bounds without another explicit capability. In addition,
the entry point may be fixed for such capabilities (for example if sealed).

_TODO: Execute is currently impossible to represent in this way, as the API is
designed. Future work could improve this, and also allow for sentries to be
represented._

## Address spaces

Capabilities can refer to a variety of address spaces, implied by permissions.
Notable address spaces:

- Virtual memory (implied by Load, Store, Execute, and related permissions).
- Object type (implied by Seal and Unseal permissions).
- Compartment ID (implied by CompartmentID permission).

To cover all possibilities, and to handle user permissions (which could refer to
application-specific address spaces), mapping classes are required to specify
(as a string) the name of the address space that they use. This string is not
interpreted at all, but can be used to display the results effectively.

## Sealed capabilties

_TODO: Currently, sealed capabilities are just ignored. It is possible to write
mappers that find them, but there is no mechanism for using unsealing
capabilities. This section is aspirational._

This tool itself operates under CHERI, and so it cannot look inside arbitrary
sealed capabilities. However, if it encounters suitable unsealing capabilities,
it will use them. That is, we consider explicit unsealing to be a graph
traversal just like exercising Load+LoadCap can be.

Note that the branches that implicitly unseal are insufficient for this
treatment; they allow unsealing only with specific constraints (such as
branching to a specific address) and therefore do not offer arbitrary
permissions over the contents. Such capabilities are the primitives used for
compartment switching, and so such exit paths will be highlighted as potential
compartment exit points.

Note that if a capability is found that can unseal these special branch types,
then they practically revert to being normal sealed capabilities, and part of
the capability graph as normal. This situation is probably undesirable outside
of compartment switchers, and may be highlighted with a warning.

## Provenance

_TODO: This section is aspirational. Currently, only the end effect is visible._

In normal use, the tool shows what capabilities exist on the various address
spaces. However, it does not show where those capabilities come from, so whilst
the tool might highlight a compartmentalisation problem — e.g. something is
accessible that shouldn't be — it cannot show where that capability came from.

To help with debugging efforts, it is possible to configure specific regions of
each address space such that all capabilities to it (and their locations) are
recorded. This is disabled by default because we expect it to have very high
overheads for realistic purecap programs.

Note that access to a given region may require multiple capabilities. For
example, one capability might allow another to be unsealed, which grants access
to a sensitive region.
