#include <functional>

int main()
{
    std::move_only_function<void() &> func([]{});
    func();
}
