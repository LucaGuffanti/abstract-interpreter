int a;
int b;

void main() {
  /*!npk a between 0 and 10 */
  /*!npk b between 0 and 2 */
  if(b == 0) {
    a = 1;
    b = a * 3;
  }
  else {
    if(b == 1) {
      a = 2;
    }
    else {
      a = 6;
    }
  }
  assert(a <= 6);
  assert(a >= 0);
  assert(b >= 1);
  assert(b <= 3);
}
