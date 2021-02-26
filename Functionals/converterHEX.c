/*#include <stdio.h>


int main(void){

    char s[5000];
    scanf("%[^\t]",s);
    printf("\n\r");
    for(int i=0; i<sizeof(s); i++){
	if(i%5 == 2){printf(" 0x");}
	if(i%5 == 0 && s[i] == '\0'){
	    printf("\n\r");
	    break;
	} 
	if(i%5 == 0){printf("0x");}
	printf("%c", s[i]);
    }

    return 0;
}
*/
#include <stdio.h>
int main(void){
    //while(1){
    	char s [50000];
    	scanf("%[^\t]",s);
    	printf("\n\r");
    	printf("{");
    	for(int i=0; i<sizeof(s); i++){
		if((i%5 == 4 || i%5 == 2) && s[i+1] == '\0'){
	    		printf("},\n\r");
	    		break;
		}
		if(i%5 == 2){printf(", 0x");}
		if(i%5 == 4){printf(",");}
		if(i%5 == 0){printf("0x");}
		printf("%c", s[i]);
    	}
	//free(s);
    //}
    	return 0;
}
