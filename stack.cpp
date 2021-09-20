#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef int StackElem;
const char* STACK_TYPE = "int";

const char* OK = '\0';
const bool FAIL = 0;
const int POISON = -666;

struct Stack {
  StackElem* data;
  int size;
  int capacity;

  struct {
    const char* name;
    const char* creationFile;
    const char* creationFunction;
    const int creationLine;
    char* dumpFile;
    char* dumpFunction;
    int dumpLine;
  } DumpInfo;
};

int StackCtor(Stack* stack);
int StackDtor(Stack* stack);

int StackPush(Stack* stack, StackElem value);
StackElem StackPop(Stack* stack);

const char *IsStackOk(const Stack* stack);
const char *IsDataOk(const Stack* stack);
const char *IsCapacityOk(const Stack* stack);
const char *IsSizeOk(const Stack* stack);
const char *IsAllOk(const Stack* stack);
int StackDump(Stack* stack, FILE* outstream = stderr);
void CheckAll(Stack* stack);

StackElem* StackIncrease(Stack* stack);
StackElem* StackDecrease(Stack* stack);

int main() {
    Stack intStack = {0, 0, 5, "intStack", __FILE__, __FUNCTION__, __LINE__ - 4, 0, 0, 0};
    StackCtor(&intStack);

    StackPush(&intStack, 5);
    StackPush(&intStack, 12);
    StackPush(&intStack, 13);

    printf("%d\n", StackPop(&intStack));
    printf("%d\n", StackPop(&intStack));

    StackDtor(&intStack);
}

int StackCtor(Stack* stack) {
    if (!stack)       assert(FAIL && "STACK_NULL");
    if (stack->capacity <= 0) assert(FAIL && "CAPACITY_ERROR");

    stack->data = (StackElem*)calloc(stack->capacity, sizeof(stack->data[0]));
    if (!stack->data) assert(FAIL && "DATA_NULL");

    for (int curIdx = 0; curIdx < stack->capacity; curIdx++) {
        stack->data[curIdx] = POISON;
    }

    stack->size = 0;

    return 1;
}

int StackDtor(Stack* stack) {
    CheckAll(stack);

    free(stack->data);
    stack->data = (StackElem*) 0xF2EE;
    stack->size = -1;

    return 1;
}

int StackPush(Stack* stack, StackElem value) {
    CheckAll(stack);

    if (stack->size >= stack->capacity) stack->data = StackIncrease(stack); //ÃÈÑÒÅÐÅÇÈÑ
    stack->data[stack->size++] = value;


    CheckAll(stack);
    return 1;
}

StackElem StackPop(Stack* stack) {
    CheckAll(stack);

    if (stack->capacity >= 2*stack->size) StackDecrease(stack);
    if (stack->size == 0) assert(0 && "STACK_UNDERFLOW");

    StackElem popped = stack->data[--stack->size];
    stack->data[stack->size] = POISON;
    return popped;
}

const char* IsStackOk(const Stack* stack) {
    if (      stack == nullptr) return "STACK_NULL";

    return OK;
}

const char* IsDataOk(const Stack* stack) {
    if (            stack->data == nullptr) return  "DATA_NULL";
    if (stack->data == (StackElem*)0xF2EE)  return "STACK_FREE";

    return OK;
}

const char* IsCapacityOk(const Stack* stack) {
    if (       stack->capacity < 0) return "CAPACITY_NEGATIVE";
    if (!isfinite(stack->capacity)) return "CAPACITY_INFINITE";

    return OK;
}

const char* IsSizeOk(const Stack* stack) {
    if (  stack->size > stack->capacity) return "STACK_OVERFLOW";
    if (stack->size < 0)                return "STACK_UNDERFLOW";

    return OK;
}

const char *IsAllOk(const Stack* stack) {
    if (    const char* StackError = IsStackOk(stack)) return StackError;
    if (    const char* StackError =  IsDataOk(stack)) return StackError;
    if (const char* StackError =  IsCapacityOk(stack)) return StackError;
    if (    const char* StackError =  IsSizeOk(stack)) return StackError;

    return OK;
}

int StackDump(Stack* stack, FILE* outstream) {
    setvbuf(outstream, NULL, _IONBF, 0);

    fprintf(outstream, "Dump from %s() at %s(%d): IsStackOk() FAILED\n", stack->DumpInfo.dumpFunction, stack->DumpInfo.dumpFile, stack->DumpInfo.dumpLine);
    fprintf(outstream, "stack <%s> [%p] (ok) \"%s\" from %s (%d), %s(): {\n", STACK_TYPE, stack, stack->DumpInfo.name, stack->DumpInfo.creationFile, stack->DumpInfo.creationLine, stack->DumpInfo.creationFunction);
    fprintf(outstream, "size = %d (%s)\n", stack->size, IsSizeOk(stack) ? IsSizeOk(stack) : "ok");
    fprintf(outstream, "capacity = %d (%s)\n\n", stack->capacity, IsCapacityOk(stack) ? IsCapacityOk(stack) : "ok");
    fprintf(outstream, "data[%p] {\n", stack->data);

    for(int curIdx = 0; curIdx < stack->capacity && stack->data != (StackElem*)0xF2EE; curIdx++) {
        if (curIdx < stack->size) {
            fprintf(outstream, "   *[%d] = %d\n", curIdx, stack->data[curIdx]);
        }
        else {
            fprintf(outstream, "   [%d] = %d (POISON)\n", curIdx, stack->data[curIdx]);
        }
    }

    fprintf(outstream, "}\n");
    /* Dump from StackPush() at main.cpp(130): OK() FAILED
    stack <int> [0x000856A] (ok) "stk" from main.cpp (25), int main(): {
    size = 3 (ok)
    capacity = 5 (ok)

    data[0x0009230] {
        *[0] = 10
        *[1]= 20
        *[2] = 30
        [3] = -666 (POISON)
        [4] = -666 (POISON)
        }
    } */
    return 1;
}

void CheckAll(Stack* stack) {
    if (const char* StackError = IsAllOk(stack)) {
        printf("Error %s, read full description in dump file\n", StackError);

        stack->DumpInfo.dumpFile = __FILE__;
        stack->DumpInfo.dumpLine = __LINE__ - 4;
        stack->DumpInfo.dumpFunction = (char*)__FUNCTION__;

        StackDump(stack);
        assert(FAIL);
    }
}

StackElem* StackIncrease(Stack* stack) {
    CheckAll(stack);

    StackElem* newPointer = (StackElem*)realloc(stack->data, stack->capacity*sizeof(stack->data[0])*3/2 + 1);

    assert(newPointer && "NO MEMORY TO INCREASE");
    //assert(newPointer == stack->data && "POINTER MOVED TO A NEW LOCATION    ");

    stack->capacity = stack->capacity*3/2 + 1;
    return newPointer;
}

StackElem* StackDecrease(Stack* stack) {
    CheckAll(stack);

    StackElem* newPointer = (StackElem*)realloc(stack->data, stack->capacity*sizeof(stack->data[0])/2);
    assert(newPointer && "MEMORY DECREASE ERROR");

    stack->capacity /= 2;
    return newPointer;
}
