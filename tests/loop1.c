int a,b;

void main(){
    a = 0;
    b = a;
    while(a < 10){
        b = b + 1;
        a = b;
    }
}