#define FUNC_MACRO(x, y) x + y
#define TEST_MACRO value

int main() {
	int TEST_MACRO = FUNC_MACRO(2, 3);
	
	return value;
}

#define HE HI
#define LLO _THERE
#define HELLO "HI THERE"
#define CAT(a,b) a##b
#define XCAT(a,b) CAT(a,b)
#define CALL(fn) fn(HE,LLO)
CAT(HE, LLO) // "HI THERE", because concatenation occurs before normal expansion
XCAT(HE, LLO) // HI_THERE, because the tokens originating from parameters ("HE" and "LLO") are expanded first
CALL(CAT) // "HI THERE", because parameters are expanded first
