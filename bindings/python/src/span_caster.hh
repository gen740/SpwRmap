#include <pybind11/cast.h>
#include <pybind11/detail/descr.h>

#include <print>
#include <span>

namespace py = pybind11;
namespace pybind11::detail {

template <class T>
struct type_caster<std::span<T>> {
  using value_conv = make_caster<T>;
  PYBIND11_TYPE_CASTER(std::span<T>, _("memoryview"));

  py::buffer owner_{};

  [[nodiscard]] bool load(handle src, bool /*convert*/) {
    if (!PyObject_CheckBuffer(src.ptr())) {
      return false;
    }
    owner_ = py::reinterpret_borrow<py::buffer>(src);
    py::buffer_info info = owner_.request();

    if (info.ndim != 1) {
      return false;
    }
    if (static_cast<std::size_t>(info.itemsize) !=
        sizeof(std::remove_const_t<T>)) {
      return false;
    }
    if (!info.strides.empty() &&
        info.strides[0] !=
            static_cast<py::ssize_t>(sizeof(std::remove_const_t<T>))) {
      return false;
    }
    if constexpr (!std::is_const_v<T>) {
      if (info.readonly) {
        return false;
      }
    }

    auto* ptr = static_cast<std::remove_const_t<T>*>(info.ptr);
    const std::size_t n = static_cast<std::size_t>(info.size);

    if constexpr (std::is_const_v<T>) {
      value = std::span<const std::remove_const_t<T>>(ptr, n);
    } else {
      value = std::span<T>(ptr, n);
    }
    return true;
  }

  static handle cast(const std::span<T>& s, return_value_policy /*policy*/,
                     handle /*parent*/) {
    using Elem = std::remove_const_t<T>;
    const py::ssize_t shape[1] = {static_cast<py::ssize_t>(s.size())};
    const py::ssize_t strides[1] = {static_cast<py::ssize_t>(sizeof(Elem))};

    if constexpr (std::is_const_v<T>) {
      return py::memoryview::from_buffer(static_cast<const Elem*>(s.data()),
                                         std::array<py::ssize_t, 1>{shape[0]},
                                         std::array<py::ssize_t, 1>{strides[0]})
          .release();
    } else {
      return py::memoryview::from_buffer(static_cast<Elem*>(s.data()),
                                         std::array<py::ssize_t, 1>{shape[0]},
                                         std::array<py::ssize_t, 1>{strides[0]},
                                         /*readonly=*/false)
          .release();
    }
  }
};

}  // namespace pybind11::detail

inline py::bytes to_py_bytes(std::span<const std::uint8_t> s) {
  return py::bytes(reinterpret_cast<const char*>(s.data()),
                   static_cast<py::ssize_t>(s.size()));
}

template <class T>
inline py::memoryview to_py_memoryview(std::span<T> s) {
  using Elem = std::remove_const_t<T>;
  const py::ssize_t shape[1] = {static_cast<py::ssize_t>(s.size())};
  const py::ssize_t strides[1] = {static_cast<py::ssize_t>(sizeof(Elem))};

  if constexpr (std::is_const_v<T>) {
    return py::memoryview::from_buffer(static_cast<const Elem*>(s.data()),
                                       std::array<py::ssize_t, 1>{shape[0]},
                                       std::array<py::ssize_t, 1>{strides[0]});
  } else {
    return py::memoryview::from_buffer(static_cast<Elem*>(s.data()),
                                       std::array<py::ssize_t, 1>{shape[0]},
                                       std::array<py::ssize_t, 1>{strides[0]},
                                       /*readonly=*/false);
  }
}
