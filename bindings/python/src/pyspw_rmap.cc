#include <pybind11/pybind11.h>

#include <print>

namespace py = pybind11;

// Base class
struct A {
  A() = default;
  A(const A &) = default;
  A(A &&) = delete;
  auto operator=(const A &) -> A & = default;
  auto operator=(A &&) -> A & = delete;
  virtual ~A() = default;
  virtual auto go() const -> void = 0;
  virtual auto test() const -> void { std::println("A::test"); }
};

// Trampoline class for Python override
struct PyA : public A {
  using A::A;

  auto go() const -> void override {
    PYBIND11_OVERRIDE_PURE(void,  // Return type
                           A,     // Parent class
                           go     // Name of the function
    );
  }

  auto test() const -> void override { PYBIND11_OVERRIDE(void, A, test); }
};

// Derived class
struct B final : public A {
  auto go() const -> void override { std::print("B::go\n"); }
};

PYBIND11_MODULE(_core, m) {
  py::class_<A, PyA>(m, "A")
      .def(py::init<>())
      .def("go", &A::go)
      .def("test", &A::test);

  py::class_<B, A>(m, "B").def(py::init<>());
}
