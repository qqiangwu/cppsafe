#pragma once

#include <type_traits>
#include <variant>

namespace cppsafe {

// llvm::PointerUnion without limits
template <class... Types> class PointerVariant {
public:
    template <class U>
    explicit PointerVariant(U&& X)
        requires(!std::is_same_v<U, PointerVariant>)
        : Data(std::forward<U>(X))
    {
    }

    template <class U>
    PointerVariant& operator=(const U& X)
        requires(!std::is_same_v<U, PointerVariant>)
    {
        Data = X;
        return *this;
    }

    bool operator==(const PointerVariant& Other) const { return Data == Other.Data; }

    bool operator<(const PointerVariant& Other) const { return Data < Other.Data; }

    template <class T> bool is() const { return std::holds_alternative<T>(Data); }

    template <class T> T get() const { return std::get<T>(Data); }

    template <class T> T dyn_cast() const // NOLINT
    {
        const auto* P = std::get_if<T>(&Data);
        if (!P) {
            return nullptr;
        }

        return *P;
    }

private:
    std::variant<Types...> Data;
};

}
