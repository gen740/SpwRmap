#include <pybind11/cast.h>
#include <pybind11/detail/descr.h>

#include <print>
#include <span>

namespace py = pybind11;
namespace pybind11::detail {

// std::span<T> type_caster
template <class T>
struct type_caster<std::span<T>> {
  using value_conv = make_caster<T>;
  // name と cast 演算子などを定義してくれる
  // PYBIND11_TYPE_CASTER(std::span<T>, _("span[") + value_conv::name + _("]"));
  PYBIND11_TYPE_CASTER(std::span<T>, _("memoryview"));

  py::buffer owner_{};

  // Python -> C++
  [[nodiscard]] bool load(handle src, bool /*convert*/) {
    if (!PyObject_CheckBuffer(src.ptr())) {
      return false;
    }
    owner_ = py::reinterpret_borrow<py::buffer>(src);
    py::buffer_info info = owner_.request();  // keep base alive for this call

    // 1D 連続のみ受け付け
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
        return false;  // 可変 span は書込可のバッファが必要
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

  // C++ -> Python：memoryview（ゼロコピー、寿命注意）
  static handle cast(const std::span<T>& s, return_value_policy /*policy*/,
                     handle /*parent*/) {
    using Elem = std::remove_const_t<T>;
    const py::ssize_t shape[1] = {static_cast<py::ssize_t>(s.size())};
    const py::ssize_t strides[1] = {static_cast<py::ssize_t>(sizeof(Elem))};

    if constexpr (std::is_const_v<T>) {
      // const T* 版
      return py::memoryview::from_buffer(static_cast<const Elem*>(s.data()),
                                         std::array<py::ssize_t, 1>{shape[0]},
                                         std::array<py::ssize_t, 1>{strides[0]})
          .release();
    } else {
      // T* 版（readonly 指定可能なオーバーロード）
      return py::memoryview::from_buffer(static_cast<Elem*>(s.data()),
                                         std::array<py::ssize_t, 1>{shape[0]},
                                         std::array<py::ssize_t, 1>{strides[0]},
                                         /*readonly=*/false)
          .release();
    }
  }
};

}  // namespace pybind11::detail

// ---- Helpers (C++ -> Python) ---------------------------------------------

// bytes にコピー（安全）
inline py::bytes to_py_bytes(std::span<const std::uint8_t> s) {
  return py::bytes(reinterpret_cast<const char*>(s.data()),
                   static_cast<py::ssize_t>(s.size()));
}

// memoryview を返す（ゼロコピー・寿命注意）
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
