#include <sigc++/signal.h>

class MinimalSignalTest {
public:
    sigc::signal<void> test_signal;
};

int main() {
    MinimalSignalTest obj;
    obj.test_signal.connect([](){ });
    obj.test_signal.emit();
    return 0;
}
