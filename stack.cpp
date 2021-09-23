#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

const int POISON = 0xE2202;

typedef int StackElem;
const char* STACK_TYPE = "int";

enum Errors {
    OK,
    STACK_OVERFLOW,
    STACK_UNDERFLOW,
    CAPACITY_NEGATIVE,
    DATA_NULL,
    STACK_FREE,
    STACK_NULL,
    CAPACITY_INFINITE
};

struct Stack {
    StackElem* data;
    int size;
    int capacity;

    struct {
        const char* name;
        const char* creationFile;
        const char* creationFunction;
        const int creationLine;
        const char * dumpFile;
        const char* dumpFunction;
        int dumpLine;
	} DumpInfo;
};

int StackCtor(Stack* stack);
int StackDtor(Stack* stack);

int StackPush(Stack* stack, StackElem value);
StackElem StackPop(Stack* stack);

enum Errors IsStackOk(const Stack* stack);
enum Errors IsDataOk(const Stack* stack);
enum Errors IsCapacityOk(const Stack* stack);
enum Errors IsSizeOk(const Stack* stack);
enum Errors IsAllOk(const Stack* stack);

int StackDump(Stack* stack, FILE* outstream = stderr);
void CheckAll(Stack* stack);
const char* ErrorToString(enum Errors error);

StackElem* StackIncrease(Stack* stack);
StackElem* StackDecrease(Stack* stack);

int main() {
    Stack intStack = {nullptr, 0, 2, "intStack", __FILE__, __FUNCTION__, __LINE__ - 4, nullptr, nullptr, 0};
    StackCtor(&intStack);

    StackPush(&intStack, 5);
    StackPush(&intStack, 12);
    StackPush(&intStack, 13);

    printf("%d\n", StackPop(&intStack));
    printf("%d\n", StackPop(&intStack));
    printf("%d\n", StackPop(&intStack));

    StackDtor(&intStack);
}

int StackCtor(Stack* stack) {
    assert(stack != nullptr && "STACK_NULL");
    assert(stack->capacity > 0 && "CAPACITY_ERROR");

    stack->data = (StackElem*)calloc(stack->capacity, sizeof(stack->data[0]));
    assert(stack->data != nullptr && "DATA_NULL");

    for (int curIdx = 0; curIdx < stack->capacity; curIdx++) {
        stack->data[curIdx] = POISON;
    }
    stack->size = 0;

    return 0;
}

int StackDtor(Stack* stack) {
    CheckAll(stack);

    free(stack->data);
    stack->data = (StackElem*) 0xF2EE;
    stack->size = -1;

    return 0;
}

int StackPush(Stack* stack, StackElem value) {
    CheckAll(stack);

    if (stack->size >= stack->capacity) stack->data = StackIncrease(stack);
    CheckAll(stack);

    stack->data[stack->size++] = value;
    CheckAll(stack);

    return 0;
}

StackElem StackPop(Stack* stack) {
    CheckAll(stack);

    if (stack->capacity >= 2*stack->size) StackDecrease(stack);
    CheckAll(stack);

    assert(stack->size != 0 && "STACK_UNDERFLOW");

    StackElem popped = stack->data[--stack->size];
    stack->data[stack->size] = POISON;

    return popped;
}

enum Errors IsStackOk(const Stack* stack) {
    if (stack == nullptr) return STACK_NULL;

    return OK;
}

enum Errors IsDataOk(const Stack* stack) {
    if (stack->data == nullptr)             return  DATA_NULL;
    if (stack->data == (StackElem*)0xF2EE)  return STACK_FREE;

    return OK;
}

enum Errors IsCapacityOk(const Stack* stack) {
    if (stack->capacity < 0)        return CAPACITY_NEGATIVE;
    if (!isfinite(stack->capacity)) return CAPACITY_INFINITE;

    return OK;
}

enum Errors IsSizeOk(const Stack* stack) {
    if (stack->size > stack->capacity)   return STACK_OVERFLOW;
    if (stack->size < 0)                 return STACK_UNDERFLOW;

    return OK;
}

enum Errors IsAllOk(const Stack* stack) {
    if (enum Errors StackError = IsStackOk(stack))     return StackError;
    if (enum Errors StackError =  IsDataOk(stack))     return StackError;
    if (enum Errors StackError =  IsCapacityOk(stack)) return StackError;
    if (enum Errors StackError =  IsSizeOk(stack))     return StackError;

    return OK;
}

int StackDump(Stack* stack, FILE* outstream) {
    setvbuf(outstream, NULL, _IONBF, 0);

    fprintf(outstream, "Dump from %s() at %s(%d): IsStackOk() FAILED\n", stack->DumpInfo.dumpFunction, stack->DumpInfo.dumpFile, stack->DumpInfo.dumpLine);
    fprintf(outstream, "stack <%s> [%p] (ok) \"%s\" from %s (%d), %s(): {\n", STACK_TYPE, stack, stack->DumpInfo.name, stack->DumpInfo.creationFile, stack->DumpInfo.creationLine, stack->DumpInfo.creationFunction);
    fprintf(outstream, "size = %d (%s)\n", stack->size, ErrorToString(IsSizeOk(stack)));
    fprintf(outstream, "capacity = %d (%s)\n\n", stack->capacity, ErrorToString(IsCapacityOk(stack)));
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
    stack <int> [0x000856A] (STACK_NULL) "stk" from main.cpp (25), int main(): {
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
    return 0;
}

void CheckAll(Stack* stack) {
    if (enum Errors StackError = IsAllOk(stack)) {
        printf("Error %s, read full description in dump file\n", ErrorToString(StackError));

        stack->DumpInfo.dumpFile = __FILE__;
        stack->DumpInfo.dumpLine = __LINE__ - 4;
        stack->DumpInfo.dumpFunction = __FUNCTION__;

        StackDump(stack);
        assert(0);
    }
}

const char* ErrorToString(enum Errors error) {
    switch(error) {
        case OK:
            return "Ok";
        case STACK_OVERFLOW:
            return "Overflow";
        case STACK_UNDERFLOW:
            return "Underflow";
        case CAPACITY_NEGATIVE:
            return "Negative capacity";
        case DATA_NULL:
            return "Data is null";
        case STACK_FREE:
            return "Stack is already free";
        case STACK_NULL:
            return "Stack is null";
        case CAPACITY_INFINITE:
            return "Capacity is infinite";
        default:
            return "Unknown error";
    }
}

StackElem* StackIncrease(Stack* stack) {
    CheckAll(stack);

    StackElem* newPointer = (StackElem*)realloc(stack->data, stack->capacity*sizeof(stack->data[0])*3/2 + 1);
    assert(newPointer != nullptr && "MEMORY_INCREASE_ERR");

    stack->capacity = stack->capacity * 3 / 2 + 1;
    return newPointer;
}

StackElem* StackDecrease(Stack* stack) {
    CheckAll(stack);

    StackElem* newPointer = (StackElem*)realloc(stack->data, stack->capacity*sizeof(stack->data[0])/2);
    assert(newPointer != nullptr && "MEMORY_DECREASE_ERROR");

    stack->capacity /= 2;
    return newPointer;
}
