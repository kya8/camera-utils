#include "test.h"
#include <slate/varmap.hpp>

using namespace slate;

namespace {

void static_checks()
{
    using namespace detail;
    static_assert(std::is_same_v<unique_variant_t<int, int>, std::variant<std::monostate, int>>);
    static_assert(std::is_same_v<unique_variant_t<int, int, float, float, int>, std::variant<std::monostate, int, float>>);
    static_assert(std::is_same_v<unique_variant_t<int, float, std::string, float, std::string, int>, std::variant<std::monostate, int, float, std::string>>);

    static_assert(get_index<int, int>() == 0);
    static_assert(get_index<int, float, char, int>() == 2);
    static_assert(get_index<int, float, int, char, int, float>() == 1);
    static_assert(get_index<int, float, float, int, char>() == 2);
}

} // namespace

template<typename T>
auto& test_lookup(const VarMap& map, const auto& ...keys)
{
    SLATE_ASSERT(map.get(keys...));
    auto p = map.get<T>(keys...);
    SLATE_ASSERT(p);
    SLATE_ASSERT_NOTHROW(map.get_ex<T>(keys...));
    return *p;
}

template<typename T>
void test_lookup_fail(const VarMap& map, const auto& ...keys)
{
    //SLATE_ASSERT(!map.get(keys...));
    SLATE_ASSERT(!map.get<T>(keys...));
    SLATE_ASSERT_THROW(map.get_ex<T>(keys...), std::out_of_range);
}

void test_varmap_lookup()
{
    const auto& model_name = "test model";

    VarMap map{{KeyId::CameraModel, model_name}, {"Answer", 42}, {"List", types::VecI32{1, 2, 3}}};
    map["Array"] = Array{3.14, "foo", "bar", false, std::vector{0.0f}};
    map["Map"] = VarMap{{KeyId::FPS, 60.0}, {KeyId::VideoTrackIds, std::vector{0, 1}}, {"Codec", "av01"}};

    auto& model = test_lookup<std::string>(map, KeyId::CameraModel);
    SLATE_ASSERT(model == model_name);
    // Yay, simple lookup works!

    test_lookup<types::VecI32>(map, "Map", KeyId::VideoTrackIds);
    test_lookup<std::string>(map, "Map", "Codec");
    auto& array = test_lookup<Array>(map, "Array");
    SLATE_ASSERT(array[0] == 3.14);

    test_lookup_fail<bool>(map, "Answer");
    test_lookup_fail<std::string>(map, KeyId::SubModel);
    test_lookup_fail<types::VecDouble>(map, "Map", KeyId::FPS);
    test_lookup_fail<types::Int>(map, "this key", "does not", KeyId::AccData, "exist");
}

int
varmap(int, char*[])
{
    static_checks();
    test_varmap_lookup();
    return 0;
}
