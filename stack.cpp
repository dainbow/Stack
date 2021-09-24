#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define LOW_LEVEL  1
#define MID_LEVEL  2
#define HIGH_LEVEL 3
#define LOCATION __FILE__, __FUNCTION__, __LINE__

#define STACK_DEBUG HIGH_LEVEL

#if !defined(STACK_DEBUG)
    #define STACK_DEBUG LOW_LEVEL
#endif

#define StackCtor(stack)                           \
    Stack stack;                                   \
    stack.creationInfo = {LOCATION, #stack};       \
    StackCtor_(&stack);

#define CheckAllStack(stack)                                                                       \
    if (Errors StackError = IsAllOk(stack)) {                                                 \
        printf("Error %s, read full description in dump file\n", ErrorToString(StackError));  \
                                                                                              \
        stack->dumpInfo = {LOCATION, #stack};                                                 \
                                                                                              \
        StackDump(stack);                                                                     \
        assert(0 && "Verify failed");                                                         \
    }

const int POISON = 0xE2202;
const long long CANARY = 0xDEADBEEFDEADBEEF;

typedef int StackElem;
const char* STACK_TYPE = "int";

typedef enum {
    OK,
    STACK_OVERFLOW,
    STACK_UNDERFLOW,
    CAPACITY_NEGATIVE,
    DATA_NULL,
    STACK_FREE,
    STACK_NULL,
    CAPACITY_INFINITE,
    LEFT_STACK_CANARY_IRRUPTION,
    RIGHT_STACK_CANARY_IRRUPTION,
    LEFT_DATA_CANARY_IRRUPTION,
    RIGHT_DATA_CANARY_IRRUPTION
} Errors;

struct VarInfo {
    const char* file;
    const char* function;
    int line;

    const char* name;
};

struct Stack {
    long long canaryLeft;

    struct VarInfo creationInfo;
    struct VarInfo dumpInfo;
    int size;
    int capacity;
    char* data;

    long long canaryRight;
};

int StackCtor_(Stack* stack);
int StackDtor(Stack* stack);

int StackPush(Stack* stack, StackElem value);
StackElem StackPop(Stack* stack);

Errors IsStackOk(const Stack* stack);
Errors IsDataOk(const Stack* stack);
Errors IsCapacityOk(const Stack* stack);
Errors IsSizeOk(const Stack* stack);
Errors IsAllOk(const Stack* stack);

int StackDump(Stack* stack, FILE* outstream = stderr);
const char* ErrorToString(Errors error);

char* StackIncrease(Stack* stack);
char* StackDecrease(Stack* stack);

int main() {
    StackCtor(intStack);

    StackPush(&intStack, 5);
    StackPush(&intStack, 12);
    StackPush(&intStack, 13);

    StackPop(&intStack);

    //intStack.dumpInfo = {LOCATION, "intStack"};
    //StackDump(&intStack);

    StackDtor(&intStack);
}

int StackCtor_(Stack* stack) {
    assert(stack != nullptr && "STACK_NULL");

    stack->canaryLeft = CANARY;

    stack->capacity = 100;
    stack->size = 0;

    stack->data = (char*)calloc(stack->capacity*sizeof(StackElem) + 2*sizeof(long long), 1);
    assert(stack->data != nullptr && "DATA_NULL");

    stack->canaryRight = CANARY;

    *(long long*)(stack->data) = CANARY;
    *(long long*)(stack->data + sizeof(long long) + stack->capacity*sizeof(StackElem)) = CANARY;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        for (int curIdx = 0; curIdx < stack->capacity; curIdx++) {
            *(StackElem*)(stack->data + sizeof(long long) + curIdx*sizeof(StackElem)) = POISON;
        }
    #endif

    return 0;
}

int StackDtor(Stack* stack) {
    CheckAllStack(stack);

    free(stack->data);
    stack->data = (char*)0xF2EE;
    stack->size = -1;

    return 0;
}

int StackPush(Stack* stack, StackElem value) {
    CheckAllStack(stack);

    if (stack->size >= stack->capacity) stack->data = StackIncrease(stack);
    *(StackElem*)(stack->data + sizeof(long long) + (stack->size++)*sizeof(StackElem)) = value;

    CheckAllStack(stack);
    return 0;
}

StackElem StackPop(Stack* stack) {
    CheckAllStack(stack);

    if (stack->capacity >= 2*stack->size) StackDecrease(stack);
    assert(stack->size != 0 && "STACK_UNDERFLOW");

    StackElem popped = *(StackElem*)(stack->data + sizeof(long long) + (--stack->size)*sizeof(StackElem));
    *(StackElem*)(stack->data + sizeof(long long) + (stack->size)*sizeof(StackElem)) = POISON;

    CheckAllStack(stack);
    return popped;
}

Errors IsStackOk(const Stack* stack) {
    if (stack == nullptr)                 return STACK_NULL;

    #if (STACK_DEBUG >= MID_LEVEL)
        if (stack->canaryLeft != CANARY)  return LEFT_STACK_CANARY_IRRUPTION;
        if (stack->canaryRight != CANARY) return RIGHT_STACK_CANARY_IRRUPTION;
    #endif

    return OK;
}

Errors IsDataOk(const Stack* stack) {
    #if (STACK_DEBUG >= MID_LEVEL)
        if (*(long long*)(stack->data) != CANARY) return LEFT_STACK_CANARY_IRRUPTION;
        if (*(long long*)(stack->data + sizeof(long long) + stack->capacity*sizeof(StackElem)) != CANARY) return RIGHT_STACK_CANARY_IRRUPTION;
    #endif

    if (stack->data == nullptr)        return  DATA_NULL;
    if (stack->data == (char*)0xF2EE)  return STACK_FREE;

    return OK;
}

Errors IsCapacityOk(const Stack* stack) {
    if (stack->capacity < 0)        return CAPACITY_NEGATIVE;
    if (!isfinite(stack->capacity)) return CAPACITY_INFINITE;

    return OK;
}

Errors IsSizeOk(const Stack* stack) {
    if (stack->size > stack->capacity)   return STACK_OVERFLOW;
    if (stack->size < 0)                 return STACK_UNDERFLOW;

    return OK;
}

Errors IsAllOk(const Stack* stack) {
    if (Errors StackError =  IsStackOk(stack))    return StackError;
    if (Errors StackError =  IsDataOk(stack))     return StackError;
    if (Errors StackError =  IsCapacityOk(stack)) return StackError;
    if (Errors StackError =  IsSizeOk(stack))     return StackError;

    return OK;
}

int StackDump(Stack* stack, FILE* outstream) {
    setvbuf(outstream, NULL, _IONBF, 0);

    fprintf(outstream, "Dump from %s() at %s(%d) in stack called now %s: IsStackOk() FAILED\n", stack->dumpInfo.function, stack->dumpInfo.file, stack->dumpInfo.line, stack->dumpInfo.name);
    fprintf(outstream, "stack <%s> [%p] (ok) \"%s\" from %s (%d), %s(): {\n", STACK_TYPE, stack, stack->creationInfo.name, stack->creationInfo.file, stack->creationInfo.line, stack->creationInfo.function);
    fprintf(outstream, "size = %d (%s)\n", stack->size, ErrorToString(IsSizeOk(stack)));
    fprintf(outstream, "capacity = %d (%s)\n\n", stack->capacity, ErrorToString(IsCapacityOk(stack)));
    fprintf(outstream, "data[%p] (%s)\n", stack->data, ErrorToString(IsDataOk(stack)));

    #if (STACK_DEBUG >= HIGH_LEVEL)
        fprintf(outstream, "{\n");
        for(int curIdx = 0; curIdx < stack->capacity; curIdx++) {
            StackElem* curElement = (StackElem*)(stack->data + sizeof(long long) + curIdx*sizeof(StackElem));
            if (curIdx < stack->size) {
                fprintf(outstream, "   *[%d][%p] = %d (%s)\n", curIdx, curElement, *curElement, (*curElement == POISON) ? "MAYBE POISON" : "OK");
            }
            else {
                fprintf(outstream, "   [%d][%p] = %d (%s)\n", curIdx, curElement, *curElement, (*curElement == POISON) ? "POISON" : "NOT POISON, BUT SHOULD BE");
            }
        }

        fprintf(outstream, "}\n");
    #endif

    return 0;
}

const char* ErrorToString(Errors error) {
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
        case LEFT_STACK_CANARY_IRRUPTION:
            return "Someone irrupted left stack canary";
        case RIGHT_STACK_CANARY_IRRUPTION:
            return "Someone irrupted right stack canary";
        case LEFT_DATA_CANARY_IRRUPTION:
            return "Someone irrupted left data canary";
        case RIGHT_DATA_CANARY_IRRUPTION:
            return "Someone irrupted right data canary";
        default:
            return "Unknown error";
    }
}

char* StackIncrease(Stack* stack) {
    CheckAllStack(stack);

    long long cellCanary = *(long long*)(stack->data + sizeof(long long) + stack->capacity*sizeof(StackElem)) = CANARY;
    *(long long*)(stack->data + sizeof(long long) + stack->capacity*sizeof(StackElem)) = 0;

    char* newPointer = (char*)realloc(stack->data, 2*sizeof(long long) + stack->capacity*sizeof(StackElem)*3/2 + 1);
    assert(newPointer != nullptr && "MEMORY_INCREASE_ERR");
    stack->capacity = stack->capacity * 3 / 2 + 1;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        for (int curIdx = stack->size + 1; curIdx < stack->capacity; curIdx++) {
            *(StackElem*)(stack->data + sizeof(long long) + curIdx*sizeof(StackElem)) = POISON;
        }
    #endif

    *(long long*)(stack->data + sizeof(long long) + stack->capacity*sizeof(StackElem)) = cellCanary;

    CheckAllStack(stack);
    return newPointer;
}

char* StackDecrease(Stack* stack) {
    CheckAllStack(stack);

    long long cellCanary = *(long long*)(stack->data + sizeof(long long) + stack->capacity*sizeof(StackElem)) = CANARY;
    *(long long*)(stack->data + sizeof(long long) + stack->capacity*sizeof(StackElem)) = 0;

    char* newPointer = (char*)realloc(stack->data, 2*sizeof(long long) + stack->capacity*sizeof(StackElem)/2);
    assert(newPointer != nullptr && "MEMORY_DECREASE_ERROR");
    stack->capacity /= 2;

    #if (STACK_DEBUG >= HIGH_LEVEL)
        for (int curIdx = stack->size + 1; curIdx < stack->capacity; curIdx++) {
            *(StackElem*)(stack->data + sizeof(long long) + curIdx*sizeof(StackElem)) = POISON;
        }
    #endif

    *(long long*)(stack->data + sizeof(long long) + stack->capacity*sizeof(StackElem)) = cellCanary;

    CheckAllStack(stack);
    return newPointer;
}
