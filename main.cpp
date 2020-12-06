#include <cassert>
#include <memory>
#include <ucontext.h>
#include <vector>

using namespace std;

const int DEFAULT_STACK_SIZE = 1024;

enum CoroutineState { Dead,
                      Ready,
                      Running,
                      Suspend };

struct Coroutine {
    ucontext_t context{};
    CoroutineState state{Dead};
    char stack[DEFAULT_STACK_SIZE]{};

    void (*func)(void *arg){};
    void *ud{};
};

struct Scheduler {

private:
    vector<Coroutine *> coroutines;

    // running_coroutine: the index of current running coroutine.
    int running_coroutine{-1};
    ucontext_t main_context{};

    // noc: num of coroutines
    // the function newCoroutine will create a new coroutine an increase the nco by 1.
    // and when the function mainFunc is finished, the nco will be decreased by 1.
    // when nco == 0, the program will exit.
    int nco{0};

public:
    void schedule() {
        while (nco > 0) {
            for (int i = 0; i < coroutines.size(); ++i) {
                if (coroutines[i]->state == Ready || coroutines[i]->state == Suspend) {
                    running_coroutine = i;
                    resume(coroutines[i]);
                }
            }
        }
    }

    static void mainFunc(Scheduler *s) {
        auto co = s->coroutines[s->running_coroutine];
        co->func(co->ud);
        co->state = Dead;
        --s->nco;
        s->running_coroutine = -1;
    }

    void newCoroutine(void (*func)(void *), void *arg) {

        int i;
        for (i = 0; i < coroutines.size(); ++i) {
            if (coroutines[i]->state == Dead) {
                break;
            }
        }

        if (i == coroutines.size()) {
            coroutines.push_back(new Coroutine());
        }

        auto co = coroutines[i];
        co->func = func;
        co->ud = arg;
        co->state = Ready;
        nco++;
    }

    void resume(Coroutine *co) {
        switch (co->state) {
            case Ready:

                getcontext(&co->context);
                co->context.uc_stack.ss_sp = co->stack;
                co->context.uc_stack.ss_size = DEFAULT_STACK_SIZE;
                co->context.uc_link = &main_context;
                makecontext(&co->context, reinterpret_cast<void (*)()>(mainFunc), 1, (void *) this);

                co->state = Running;
                swapcontext(&main_context, &co->context);
                break;
            case Suspend:
                co->state = Running;
                swapcontext(&main_context, &co->context);
                break;
            default:
                running_coroutine = -1;
                assert(0);
        }
    }

    void yield() {
        assert(running_coroutine != -1);

        auto co = coroutines[running_coroutine];
        co->state = Suspend;
        running_coroutine = -1;
        swapcontext(&co->context, &main_context);
    }
};


Scheduler s;


void print1(void *arg) {
    printf("1\n");
    s.yield();
    printf("11\n");
}

void print2(void *arg) {
    s.yield();
    printf("2\n");
}


int main() {


    s.newCoroutine(print1, nullptr);
    s.newCoroutine(print2, nullptr);

    s.schedule();

    return 0;
}
