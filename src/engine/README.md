# engine/

`engine/` is the directory for components managing objects:

*   loaded from the LM data
*   used by converters like Converter, Predictor, Rewriter, etc.

## Engine

`Engine` takes responsibility for:

*   interface to provide a `Converter` object (mainly for `Session`)
*   ownership of the `Modules` object and objects created from the `Modules`
    object
*   loading a LM from a data file

## Modules

`Modules` takes ownership of objects initialized from `DataManager`. When a new
LM is loaded, a new `Modules` object is created as a new `DataManager` object is
created.

## DataLoader

`DataLoader` loads a LM data file and create a `Modules` object. `DataLoader`
receives requests for LM data loadings and manages the priority among the
multiple requests.

### Priority of data loading

Each data loading request has a priority based on the request type (e.g. 10, 40,
100). The lower value has the higher priority. The new LM is loaded only when
the priority of the new data is higher the current LM.

`DataLoader` always processes the highest priority request. The caller can get
the highest priority LM from the `DataLoader`. (Note, it is not true yet for the
current implementation. this is one of the goal of the refactoring).

### Timing of data loading

When `DataLoader` receives a loading request, `DataLoader` will:

*   Do nothing, if the priority of the new request is lower than the priority of
    the current LM. In this case, the following items are not applicable.
*   Wait for the current loading task, if `DataLodear` is loading another LM
    now.
*   Start a new loading task for the new request, if the new request has the
    higher priority than the loaded LM.

### Concurrency policy of DataLoader

While `DataLoader` uses a sub-thread to load a LM data file, it is designed to
be thread-compatible similarly to std::vector and other Mozc objects. It means
that callers can adopt the same concurrency policy with other objects (you don't
need extra care about this object). It should be fine while you use it in a
single thread, and you should have some treatments as well as other components
if you use it in multiple threads.
