namespace A { namespace { static int hid(); int anon() { return 0; } }
class B : public C {
public:
    B();
    virtual ~B();
    virtual int f(int x = 0) const;
    int g() { return 1; }
    int m;
private:
    struct Inner { int q; } inner;
    template <class T> T t(T v) { return v; }
};
B::B() : m(0) {}
B::~B() {}
int B::f(int x) const { return x; }
bool operator==(const B &a, const B &b) { return true; }
int B::operator()(int a) { return a; }
template <typename T> struct TT { T v; };
template <> int spec<int>(int a) { return a; }
const B B::inst(&B::m);
}
