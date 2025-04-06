#pragma once

template<typename BaseType>
class EnableSelfHelper : public std::enable_shared_from_this<BaseType> {

public:

    template<typename T>
        requires std::is_base_of_v<BaseType, T>
    std::shared_ptr<T> keep_alive_this() {
        return std::static_pointer_cast<T>(this->shared_from_this());
    }

    template<typename T>
        requires std::is_base_of_v<BaseType, T>
    std::shared_ptr<const T> keep_alive_this() const {
        return std::static_pointer_cast<const T>(this->shared_from_this());
    }

    template<typename T>
        requires std::is_base_of_v<BaseType, T>
    std::weak_ptr<T> weak_from_this_cast() {
        return keep_alive_this<T>();
    }

    template<typename T>
        requires std::is_base_of_v<BaseType, T>
    std::weak_ptr<const T> weak_from_this_cast() const {
        return keep_alive_this<T>();
    }
};