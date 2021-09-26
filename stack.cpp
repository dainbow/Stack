#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LOW_LEVEL  1
#define MID_LEVEL  2
#define HIGH_LEVEL 3

#define LOCATION __FILE__, __FUNCTION__, __LINE__

#define STACK_DEBUG 3

#ifndef STACK_DEBUG
	#define STACK_DEBUG LOW_LEVEL
#endif

#define StackCtor(stack)						   \
	Stack stack;								   \
	stack.creationInfo = {LOCATION, #stack};	   \
	StackCtor_(&stack);

#define CheckAllStack(stack)																  \
	if (StackError error = IsAllOk(stack)) {												  \
		printf("Error %s, read full description in dump file\n", ErrorToString(error));		  \
																							  \
		stack->dumpInfo = {LOCATION, #stack};												  \
																							  \
		StackDump(stack);																	  \
		assert(0 && "Verify failed");														  \
	}

typedef uint32_t canary;
typedef uint64_t hashValue;
typedef int32_t StackElem;

const uint32_t POISON = 0xE2202;
const canary CANARY = 0xDEADBEEF;
const uint32_t FREE_VALUE = 0xF2EE;

const uint32_t STACK_BEGINNING_CAPACITY = 50;
const uint32_t HASH_BASE = 257;

const float INCREASE_MULTIPLIER = 1.5;
const float DECREASE_MULTIPLIER = 2;

const char* NAME_OF_STACK_TYPE = "int";

enum StackError {
	NO_ERROR = 0,
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
	RIGHT_DATA_CANARY_IRRUPTION,
	STACK_HASH_IRRUPTION,
	DATA_HASH_IRRUPTION
};

struct VarInfo {
	const char* file;
	const char* function;
	int line;

	const char* name;
};

struct Stack {
	canary canaryLeft;

	struct VarInfo creationInfo;
	struct VarInfo dumpInfo;
	int32_t size;
	int32_t capacity;
	uint8_t* data;

	canary canaryRight;
};

int32_t StackCtor_(Stack* stack);
int32_t StackDtor(Stack* stack);

int32_t StackPush(Stack* stack, StackElem pushedValue);
StackElem StackPop(Stack* stack);

StackError IsStackOk(Stack* stack);
StackError IsDataOk(Stack* stack);
StackError IsCapacityOk(Stack* stack);
StackError IsSizeOk(Stack* stack);
StackError IsAllOk(Stack* stack);

uint64_t GetHash(uint8_t* pointer, uint32_t size);
uint64_t GetStackHash(Stack* stack);
uint64_t GetDataHash(Stack* stack);
void WriteAllStackHash(Stack* stack);

int StackDump(Stack* stack, FILE* outstream = stderr);
const char* ErrorToString(StackError error);

uint8_t* StackIncrease(Stack* stack);
uint8_t* StackDecrease(Stack* stack);

uint64_t powllu(int32_t base, int32_t power);

int main() {
	StackCtor(intStack);

	StackPush(&intStack, 5);
	StackPush(&intStack, 12);
	StackPush(&intStack, 17);

	StackPush(&intStack, 13);

	printf("%d\n", StackPop(&intStack));
	printf("%d\n", StackPop(&intStack));
	printf("%d\n", StackPop(&intStack));
	printf("%d\n", StackPop(&intStack));

	StackDtor(&intStack);
}

int32_t StackCtor_(Stack* stack) {
	assert(stack != nullptr && "STACK_NULL");

	stack->canaryLeft  = CANARY;
	stack->canaryRight = CANARY;
	stack->capacity	   = STACK_BEGINNING_CAPACITY;
	stack->size		   = 0;

	uint32_t sizeOfData				= stack->capacity * sizeof(StackElem);
	uint32_t sizeOfDataProtection	= 2 * (sizeof(canary) + sizeof(hashValue));
	uint32_t sizeOfAllData			= sizeOfData + sizeOfDataProtection;

	stack->data = (uint8_t*)calloc(sizeOfAllData, sizeof(stack->data[0]));
	assert(stack->data != nullptr && "DATA_NULL");

	uint8_t* beginningOfData = stack->data + sizeof(canary) + 2 * sizeof(hashValue);
	uint8_t* endOfData		 = beginningOfData + sizeOfData;

	*(canary*)(stack->data)	 = CANARY;
	*(canary*)(endOfData)	 = CANARY;

	#if (STACK_DEBUG >= HIGH_LEVEL)
		for (int32_t curIdx = 0; curIdx < stack->capacity; curIdx++) {
			*(StackElem*)(beginningOfData + curIdx * sizeof(StackElem)) = POISON;
		}

		WriteAllStackHash(stack);
	#endif

	return 0;
}

int32_t StackDtor(Stack* stack) {
	CheckAllStack(stack);

	free(stack->data);
	stack->data = (uint8_t*)FREE_VALUE;
	stack->size = -1;

	return 0;
}

int32_t StackPush(Stack* stack, StackElem pushedValue) {
	CheckAllStack(stack);
	uint8_t* beginningOfData = stack->data + sizeof(canary) + 2 * sizeof(hashValue);

	if (stack->size >= stack->capacity) stack->data = StackIncrease(stack);

	*(StackElem*)(beginningOfData + (stack->size) * sizeof(StackElem)) = pushedValue;
	stack->size++;

	#if (STACK_DEBUG >= HIGH_LEVEL)
		WriteAllStackHash(stack);
	#endif

	CheckAllStack(stack);
	return 0;
}

StackElem StackPop(Stack* stack) {
	CheckAllStack(stack);
	uint8_t* beginningOfData = stack->data + sizeof(canary) + 2 * sizeof(hashValue);

	if (stack->capacity >= (int32_t)DECREASE_MULTIPLIER * stack->size) StackDecrease(stack);
	assert(stack->size != 0 && "STACK_UNDERFLOW");

	--stack->size;
	StackElem poppedValue = *(StackElem*)(beginningOfData + stack->size * sizeof(StackElem));
	*(StackElem*)(beginningOfData + (stack->size) * sizeof(StackElem)) = POISON;

	#if (STACK_DEBUG >= HIGH_LEVEL)
		WriteAllStackHash(stack);
	#endif

	CheckAllStack(stack);
	return poppedValue;
}

StackError IsStackOk(Stack* stack) {
	if (stack == nullptr)					  return STACK_NULL;

	#if (STACK_DEBUG >= HIGH_LEVEL)
		hashValue stackHash	 = *(hashValue*)(stack->data + sizeof(canary));
		if (GetStackHash(stack) != stackHash) return STACK_HASH_IRRUPTION;
	#endif

	#if (STACK_DEBUG >= MID_LEVEL)
		if (stack->canaryLeft  != CANARY)	  return LEFT_STACK_CANARY_IRRUPTION;
		if (stack->canaryRight != CANARY)	  return RIGHT_STACK_CANARY_IRRUPTION;
	#endif

	return NO_ERROR;
}

StackError IsDataOk(Stack* stack) {
	if (stack->data == nullptr)				return	DATA_NULL;
	if (stack->data == (uint8_t*)0xF2EE)	return STACK_FREE;

	#if (STACK_DEBUG >= HIGH_LEVEL)
		hashValue dataHash = *(hashValue*)(stack->data + sizeof(canary) + sizeof(hashValue));
		if (GetDataHash(stack) != dataHash) return DATA_HASH_IRRUPTION;
	#endif

	#if (STACK_DEBUG >= MID_LEVEL)
		uint8_t* beginningOfData = stack->data + sizeof(canary) + 2 * sizeof(hashValue);
		uint32_t sizeOfData = stack->capacity * sizeof(StackElem);

		if (*(canary*)(stack->data) != CANARY)					return LEFT_DATA_CANARY_IRRUPTION;
		if (*(canary*)(beginningOfData + sizeOfData) != CANARY) return RIGHT_DATA_CANARY_IRRUPTION;
	#endif

	return NO_ERROR;
}

StackError IsCapacityOk(Stack* stack) {
	if (stack->capacity < 0)		return CAPACITY_NEGATIVE;
	if (!isfinite(stack->capacity)) return CAPACITY_INFINITE;

	return NO_ERROR;
}

StackError IsSizeOk(Stack* stack) {
	if (stack->size > stack->capacity)	 return STACK_OVERFLOW;
	if (stack->size < 0)				 return STACK_UNDERFLOW;

	return NO_ERROR;
}

StackError IsAllOk(Stack* stack) {
	if (StackError error =	IsStackOk(stack))	 return error;
	if (StackError error =	IsDataOk(stack))	 return error;
	if (StackError error =	IsCapacityOk(stack)) return error;
	if (StackError error =	IsSizeOk(stack))	 return error;

	return NO_ERROR;
}

uint64_t GetHash(uint8_t* pointer, uint32_t size) {
	assert(pointer != nullptr);
	assert(size > 0);

	uint64_t hashSum = 0;
	for (uint32_t curByte = 0; curByte < size; curByte++) {
		hashSum += *(pointer + curByte) * powllu(HASH_BASE, curByte);
	}

	return hashSum;
}

uint64_t GetStackHash(Stack* stack) {
	uint8_t* beginningOfStack = (uint8_t*)stack + sizeof(canary);
	uint32_t sizeOfStack	  = sizeof(Stack) - 2 * sizeof(canary);

	return GetHash(beginningOfStack, sizeOfStack);
}

uint64_t GetDataHash(Stack* stack) {
	uint8_t* beginningOfData = stack->data + sizeof(canary) + 2 * sizeof(hashValue);
	uint32_t sizeOfData		 = stack->capacity * sizeof(StackElem);

	return GetHash(beginningOfData, sizeOfData);
}

void WriteAllStackHash(Stack* stack) {
	hashValue* stackHashLocation = (hashValue*)(stack->data + sizeof(canary));
	*stackHashLocation = GetStackHash(stack);

	hashValue* dataHashLocation = (hashValue*)(stack->data + sizeof(canary) + sizeof(hashValue));
	*dataHashLocation  = GetDataHash(stack);
}

int StackDump(Stack* stack, FILE* outstream) {
	setvbuf(outstream, NULL, _IONBF, 0);

	fprintf(outstream, "Dump from %s() at %s(%d) in stack called now \"%s\": IsStackOk() FAILED\n",
			stack->dumpInfo.function, stack->dumpInfo.file, stack->dumpInfo.line, stack->dumpInfo.name);

	fprintf(outstream, "stack <%s> [%p] (ok) \"%s\" ",
			NAME_OF_STACK_TYPE, stack, stack->creationInfo.name);
	fprintf(outstream, "from %s (%d), %s(): {\n",
			stack->creationInfo.file,  stack->creationInfo.line, stack->creationInfo.function);

	fprintf(outstream, "size	 = %d (%s)\n",
			stack->size,	 ErrorToString(IsSizeOk(stack)));
	fprintf(outstream, "capacity = %d (%s)\n\n",
			stack->capacity, ErrorToString(IsCapacityOk(stack)));

	#if (STACK_DEBUG >= MID_LEVEL)
		fprintf(outstream, "Stack canaries:\n");
		fprintf(outstream, "	canaryLeft[%p] = %d (%s)\n",
				&stack->canaryLeft,	 stack->canaryLeft,	 (stack->canaryLeft	 == CANARY) ?
				"Ok" : "IRRUPTION");
		fprintf(outstream, "	canaryRight[%p] = %d (%s)\n",
				&stack->canaryRight, stack->canaryRight, (stack->canaryRight == CANARY) ?
				"Ok" : "IRRUPTION");

		canary* leftDataCanaryLocation	= (canary*)stack->data;
		canary* rightDataCanaryLocation = (canary*)(stack->data + sizeof(canary) + 2 * sizeof(hashValue) + stack->capacity * sizeof(StackElem));
		fprintf(outstream, "Data canaries:\n");
		fprintf(outstream, "	canaryLeft[%p] = %d (%s)\n",
				leftDataCanaryLocation,	 *leftDataCanaryLocation,  (*leftDataCanaryLocation	 == CANARY) ?
				"Ok" : "IRRUPTION");
		fprintf(outstream, "	canaryRight[%p] = %d (%s)\n\n",
				rightDataCanaryLocation, *rightDataCanaryLocation, (*rightDataCanaryLocation == CANARY) ?
				"Ok" : "IRRUPTION");
	#endif

	#if (STACK_DEBUG >= HIGH_LEVEL)
		hashValue* stackHashLocation = (hashValue*)(stack->data + sizeof(canary));
		uint64_t curHash = GetStackHash(stack);

		fprintf(outstream, "Stack hashes:\n");
		fprintf(outstream, "	Stored stack hash[%p] = %I64d\n",
				stackHashLocation, *stackHashLocation);
		fprintf(outstream, "	Current stack hash = %I64d\n", curHash);
		fprintf(outstream, "	%s\n", (*stackHashLocation == curHash) ?
				"(Hashes are equal)" : "(HASHES AREN'T EQUAL)");

		hashValue* dataHashLocation = (hashValue*)(stack->data + sizeof(canary) + sizeof(hashValue));
		curHash = GetDataHash(stack);
		fprintf(outstream, "Data hashes:\n");
		fprintf(outstream, "	Stored data hash[%p] = %I64d\n",
				dataHashLocation, *dataHashLocation);
		fprintf(outstream, "	Current data hash = %I64d\n", curHash);
		fprintf(outstream, "	%s\n\n",
				(*dataHashLocation == curHash) ?
				"(Hashes are equal)" : "(HASHES AREN'T EQUAL)");
	#endif

	fprintf(outstream, "data[%p] (%s)\n",
			stack->data, ErrorToString(IsDataOk(stack)));
	#if (STACK_DEBUG >= HIGH_LEVEL)
		uint8_t* beginningOfData = stack->data + sizeof(canary) + 2 * sizeof(hashValue);
		fprintf(outstream, "{\n");

		for(int32_t curIdx = 0; curIdx < stack->capacity; curIdx++) {
			StackElem* curElement = (StackElem*)(beginningOfData + curIdx * sizeof(StackElem));

			if (curIdx < stack->size) {
				fprintf(outstream, "   *[%d][%p] = %d (%s)\n",
						curIdx, curElement, *curElement, (*curElement == POISON) ?
						"MAYBE POISON" : "Ok");
			}
			else {
				fprintf(outstream, "   [%d][%p] = %d (%s)\n",
						curIdx, curElement, *curElement, (*curElement == POISON) ?
						"Poison" : "NOT POISON, BUT SHOULD BE");
			}
		}

		fprintf(outstream, "}\n");
	#endif

	return 0;
}

const char* ErrorToString(StackError error) {
	switch(error) {
		case NO_ERROR:						return "Ok";
		case STACK_OVERFLOW:				return "Overflow";
		case STACK_UNDERFLOW:				return "Underflow";
		case CAPACITY_NEGATIVE:				return "Negative capacity";
		case DATA_NULL:						return "Data is null";
		case STACK_FREE:					return "Stack is already free";
		case STACK_NULL:					return "Stack is null";
		case CAPACITY_INFINITE:				return "Capacity is infinite";
		case LEFT_STACK_CANARY_IRRUPTION:	return "Someone irrupted left stack canary";
		case RIGHT_STACK_CANARY_IRRUPTION:	return "Someone irrupted right stack canary";
		case LEFT_DATA_CANARY_IRRUPTION:	return "Someone irrupted left data canary";
		case RIGHT_DATA_CANARY_IRRUPTION:	return "Someone irrupted right data canary";
		case STACK_HASH_IRRUPTION:			return "Someone irrupted stack";
		case DATA_HASH_IRRUPTION:			return "Someone irrupted data";

		default:							return "Unknown error";
	}
}

uint8_t* StackIncrease(Stack* stack) {
	CheckAllStack(stack);
	uint32_t sizeOfData				= stack->capacity * sizeof(StackElem);
	uint32_t sizeOfDataProtection	= 2 * (sizeof(canary) + sizeof(hashValue));
	uint32_t sizeOfIncreasedData	= sizeOfData * (uint32_t)INCREASE_MULTIPLIER + 1;

	uint8_t* beginningOfData		= stack->data + sizeof(canary) + 2 * sizeof(hashValue);
	canary* rightDataCanaryLocation = (canary*)(beginningOfData + sizeOfData);

	canary cellCanary = *rightDataCanaryLocation;
	*rightDataCanaryLocation = 0;

	uint8_t* newPointer = (uint8_t*)realloc(stack->data, sizeOfDataProtection + sizeOfIncreasedData);
	assert(newPointer  != nullptr && "MEMORY_INCREASE_ERR");
	stack->capacity		= stack->capacity * (int32_t)INCREASE_MULTIPLIER + 1;

	beginningOfData			 = stack->data + sizeof(canary) + 2 * sizeof(hashValue);
	sizeOfData				 = stack->capacity * sizeof(StackElem);
	rightDataCanaryLocation	 = (canary*)(beginningOfData + sizeOfData);

	#if (STACK_DEBUG >= HIGH_LEVEL)
		for (int32_t curIdx	 = stack->size + 1; curIdx < stack->capacity; curIdx++) {
			*(StackElem*)(beginningOfData + curIdx*sizeof(StackElem)) = POISON;
		}
	#endif

	*rightDataCanaryLocation = cellCanary;

	#if (STACK_DEBUG >= HIGH_LEVEL)
		WriteAllStackHash(stack);
	#endif

	CheckAllStack(stack);

	return newPointer;
}

uint8_t* StackDecrease(Stack* stack) {
	CheckAllStack(stack);
	uint32_t sizeOfData				= stack->capacity * sizeof(StackElem);
	uint32_t sizeOfDataProtection	= 2 * (sizeof(canary) + sizeof(hashValue));
	uint32_t sizeOfDecreasedData	= sizeOfData / (uint32_t)DECREASE_MULTIPLIER;

	uint8_t* beginningOfData		= stack->data + sizeof(canary) + 2 * sizeof(hashValue);
	canary* rightDataCanaryLocation = (canary*)(beginningOfData + sizeOfData);

	canary cellCanary	= *rightDataCanaryLocation;
	*rightDataCanaryLocation = 0;

	uint8_t* newPointer = (uint8_t*)realloc(stack->data, sizeOfDataProtection + sizeOfDecreasedData);
	assert(newPointer  != nullptr && "MEMORY_DECREASE_ERROR");
	stack->capacity	   /= (int32_t)DECREASE_MULTIPLIER;

	beginningOfData			 = stack->data + sizeof(canary) + 2 * sizeof(hashValue);
	sizeOfData				 = stack->capacity * sizeof(StackElem);
	rightDataCanaryLocation	 = (canary*)(beginningOfData + sizeOfData);

	#if (STACK_DEBUG >= HIGH_LEVEL)
		for (int32_t curIdx	 = stack->size + 1; curIdx < stack->capacity; curIdx++) {
			*(StackElem*)(beginningOfData + curIdx * sizeof(StackElem)) = POISON;
		}
	#endif

	*rightDataCanaryLocation = cellCanary;

	#if (STACK_DEBUG >= HIGH_LEVEL)
		WriteAllStackHash(stack);
	#endif

	CheckAllStack(stack);

	return newPointer;
}

uint64_t powllu(int32_t base, int32_t power) {
	uint64_t result = 1;

	for (int32_t curMulti = 0; curMulti < power; curMulti++) {
		result *= base;
	}

	return result;
}
