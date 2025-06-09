#pragma once
#include <pqxx/pqxx>
#include <functional>
#include <iostream>

namespace vh::database {
    template<typename Func>
    auto runTransaction(const std::string &context, Func &&func) -> decltype(func(std::declval<pqxx::work &>()));
}