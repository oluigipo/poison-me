# 19 "tests/pp-test.c"
const char* name ="pepe";
void* hello(void)
{
 return "world";
}

int an_int = 0xffffULL;
float a_float = 35.0f;
float wtf = 0x8080.0p+3f;

const char* a_string = "pepe, " STR(MY_MACRO) " is " STR2(MY_MACRO);
# 37 "tests/pp-test.c"
int a = "HI THERE";
int b = HI_THERE;
int c = "HI THERE";
# 45 "tests/pp-test.c"
int main(int argc, char* argv[])
{
 int typedef a;
 a f(void);
 a v(void);

 a (*pp[2])(void) = {
  f, v
 };

 pp[1] = &argc;

) printf("address of 'f' is %p\n", pp[0]); LOG("this is line " STR2(__LINE__) "!\n");})