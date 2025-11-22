# TODO

1. Rework the Python bindings (`bindings/python/src/pyspw_rmap.cc`) to guarantee thread termination and to catch exceptions inside the worker loop so `std::terminate` cannot be triggered during module shutdown.
2. Expand documentation (README) with build/test instructions and consider enabling tests by default in CMake, so users can run the verification suite without digging through options.
