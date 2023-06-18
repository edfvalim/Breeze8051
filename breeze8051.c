/*********************************************************************
 * 
 * Breeze8051: Sistema Adaptativo de Controle de Temperatura
 *
 * Este projeto, desenvolvido para a disciplina de Microcontroladores e
 * Aplicações (MiCAP) do curso de Engenharia de Computação na UFMS,
 * utiliza um microcontrolador C8051F040 na placa didática BIG8051.
 *
 * O sistema permite o controle de temperatura dentro de um gabinete
 * por meio de um sensor DS18B20 e uma ventoinha NOCTUA NF-P12 redux.
 *
 * Os limites de temperatura, que podem ser ajustados por teclas na 
 * placa, são exibidos em um GLCD e salvos na EEPROM da placa para 
 * referência futura.
 *
 * O objetivo é demonstrar a aplicação prática das técnicas aprendidas
 * no curso e fornecer uma solução eficaz para a regulagem de 
 * temperatura em gabinetes.
 * 
 * Alunos:
 * Eduardo Ferreira Valim
 * Gregorio Koslinski Neto
 * João Pedro de Melo Roberto
 *
 * Professor:
 * Fábio Iaione
 * 
 ********************************************************************/


#include "config.c"
#include "def_pinos.h"
#include "fonte.c"
#include <stdio.h>

#define NOP4()	NOP();NOP();NOP();NOP()
#define NOP8()	NOP4();NOP4()
#define NOP12()	NOP4();NOP8()

/*********************************************************************
 * 
 * 	GLCD
 * 
 ********************************************************************/

#define CS1 P2_0
#define CS2 P2_1
#define RS P2_2
#define RW P2_3
#define E P2_4
#define RST P2_5
#define DB P4
#define ESQ 0
#define DIR 1

#define NI 0 // Nibble
#define BY 1 // Byte
#define CO 0 // Comando
#define DA 1 // Dado

void conf_Y(unsigned char y, __bit cs);
void conf_pag(unsigned char pag, __bit cs);
void esc_glcd(unsigned char dado, __bit cd, __bit cs);
void limpa_glcd(__bit cs);
void ini_glcd(void);
unsigned char le_glcd( __bit cd, __bit cs);
int le_teclas (int porta1);

/*********************************************************************
 * 
 * 	DELAY
 * 
 ********************************************************************/

void delay_ms(unsigned int t); // Delay em milisegundos
void delay_us(unsigned int t); // Delay em microsegundos

/*********************************************************************
 * 
 * 	I2C
 * 
*********************************************************************/

#define R 1
#define W 0
unsigned char esc_byte_cntr(unsigned char end_disp, __bit RW);
unsigned char esc_byte_dado(unsigned char dado);
int esc_eeprom(unsigned char end_disp,unsigned char end, unsigned char dado);
int le_eeprom(unsigned char end_disp, unsigned char end);
float le_temp(float temp);

/*********************************************************************
 * 
 * 	DS18B20
 * 
 ********************************************************************/

#define DQ P2_7
void Init_DS1820(void);
float ReadTemperature(void);
unsigned char ReadOneChar(void);
void write_bit(unsigned char dat);
void WriteOneChar(unsigned char dat);
void Write0(void);
void Write1(void);
unsigned char ReadOneChar(void);

/*********************************************************************
 * 
 * FAN PWM
 * 
 ********************************************************************/
void setPWM(unsigned char dutyCycle);
void controlFan(float temp, int limite, float histerese);

/*********************************************************************
 * 
 * 	MAIN
 * 
 ********************************************************************/

int main(void)
{
	unsigned int cont;
	int limite;
	float temp, histerese = 5.0; // Define a histerese de 5 graus

	Init_Device();
	SFRPAGE=LEGACY_PAGE;

	ini_glcd();
	limpa_glcd(DIR);
	limpa_glcd(ESQ);

    temp = ReadTemperature();
	delay_ms(500);

    while(1)
	{
		limite = le_eeprom(0xA0,0x07);
		temp = ReadTemperature();
        cont = le_teclas(P1);

		if (cont != 21)
		{
			if (cont == 9)
                limite++;
            else if(cont == 10)
				limite--;
            cont = esc_eeprom(0xA0,0x07,limite);
		}

		controlFan(temp, limite, histerese);

        printf_fast_f("\x01 Limite = %3.1f C", limite);
		printf_fast_f("\x03 Temperatura:");
		printf_fast_f("\x04 %3.1f C", temp);
		printf_fast_f("\x06 P1_1: Aumentar ");
        printf_fast_f("\x07 P1_2: Diminuir ");
	}
}

/*********************************************************************
 * 
 * 	DELAY
 * 
 ********************************************************************/

void delay_ms(unsigned int t)  // Conta t milisegundos
{

	TMOD = TMOD | 0x01;
	TMOD = TMOD & ~0x02;
	for(;t>0; t--)
	{
		TR0 = 0;
		TF0 = 0;
		TH0 = 0x9e;
		TL0 = 0x58;
		TR0 = 1;    // Aciona o temporizador

		while(TF0 == 0);   // Permanece no estado at  que alcance o overflow
	}
}
void delay_us(unsigned int t)
{

	TMOD = TMOD | 0x01;
	TMOD = TMOD & ~0x02;
	for(;t>0; t--)
	{
		TR0 = 0;
		TF0 = 0;
		TH0 = 0xff;
		TL0 = 0xe7;
		TR0 = 1;
		while(TF0 == 0);
	}
}

/*********************************************************************
 * 
 * 	L TECLAS
 * 
 ********************************************************************/

int le_teclas (int porta1)
{
	int c2=255-porta1;
	int i = 0;

	while(i != 21)
	{
		c2=255-porta1;
		delay_ms(20);
		switch(c2)
		{
			case 1:
				return 8;
				break;
			case 2:
				return 9;
				break;
			case 4:
				return 10;
				break;
			case 8:
				return 11;
				break;
			case 16:
				return 12;
				break;
			case 32:
				return 13;
				break;
			case 64:
				return 14;
				break;
			case 128:
				return 15;
				break;
			default:
				i = 21;
				return 21;
			}
		}
}


/*********************************************************************
 * 
 * 	GLCD
 * 
 ********************************************************************/

unsigned char le_glcd( __bit cd, __bit cs)
{
	unsigned char byte;

	RW = 1;
	RS = cd;
	CS1 = cs;
	CS2 = !cs;
	NOP4();
	E = 1;
	NOP8();
	SFRPAGE=CONFIG_PAGE;
	byte = DB;
	SFRPAGE=LEGACY_PAGE;
	NOP4();
	E = 0;
	NOP12();
	return (byte);
}
void esc_glcd(unsigned char dado, __bit cd, __bit cs)
{
	while(le_glcd(CO,cs) & 0x80); // Ler estado para verificar se pode escrever
	RW = 0;
	CS1 = cs;
	CS2 = !cs;
	RS = cd;
	SFRPAGE=CONFIG_PAGE;
	DB = dado;
	SFRPAGE=LEGACY_PAGE;
	NOP4();
	E = 1;
	NOP12();
	E = 0;
	SFRPAGE=CONFIG_PAGE;
	DB = 0xff;     // Precisa fazer isso para o transistor ficar aberto para o DB ser modificado
	SFRPAGE=LEGACY_PAGE;
	NOP12();
}

void ini_glcd(void)
{
	E = 0;
	RST = 1;
	CS1 = 1;
	CS2 = 1;
	SFRPAGE=CONFIG_PAGE;
	DB = 0xff;
	SFRPAGE=LEGACY_PAGE;
	while(le_glcd(CO,ESQ) & 0x10);
	while(le_glcd(CO,DIR) & 0x10);
	esc_glcd(0x3f,CO,ESQ);
	esc_glcd(0x3f,CO,DIR);
	esc_glcd(0x40,CO,ESQ);
	esc_glcd(0x40,CO,DIR);
	esc_glcd(0xb8,CO,ESQ);
	esc_glcd(0xb8,CO,DIR);
	esc_glcd(0xc0,CO,ESQ);
	esc_glcd(0xc0,CO,DIR);
}

void conf_Y(unsigned char y, __bit cs)
{
	y &= 0x3f;
	esc_glcd(0x40|y, CO,cs);
}

void conf_pag(unsigned char pag, __bit cs)
{
	pag &= 0x07;
	esc_glcd(0xb8|pag, CO, cs);
}

void limpa_glcd(__bit cs)
{
	unsigned char i,j;
	for(i = 0; i<0x08;i++)
	{
		conf_pag(i,cs);
		conf_Y(0,cs);
		for (j=0; j<64;j++)
			esc_glcd(0x00,DA,cs);
	}
}
void putchar ( char c)
{
	__bit lado;
	static unsigned char cont_car=0;

	if (c<9)
	{
		conf_pag(c-1,ESQ);
		conf_pag(c-1,DIR);
		conf_Y(0,ESQ);
		conf_Y(0,DIR);
		cont_car=0;
	}
	else
	{
		if (cont_car<8) lado =ESQ;
			else lado =DIR;
		c = c-32;
		esc_glcd(fonte[c][0], DA, lado);
		esc_glcd(fonte[c][1], DA, lado);
		esc_glcd(fonte[c][2], DA, lado);
		esc_glcd(fonte[c][3], DA, lado);
		esc_glcd(fonte[c][4], DA, lado);
		esc_glcd(0x00, DA, lado);
		esc_glcd(0x00, DA, lado);
		esc_glcd(0x00, DA, lado);
		cont_car++;
	}
}

/*********************************************************************
 * 
 * 	I2C
 * 
 ********************************************************************/

unsigned char esc_byte_cntr(unsigned char end_disp, __bit RW)
{
    // Gerando um start
	STA=1;			// Start 1 de 3
	SI=0;			// Start 2 de 3
	while(SI==0);	// Start 3 de 3

	//verifica status
	if(SMB0STA != 0x08 && SMB0STA != 0x10)
	{
		return SMB0STA; // Erro
	}

    // Transmitindo um byte
	SMB0DAT = (end_disp & 0xfe)|RW; // Transmite o endereco 0, zerar o ultimo bit
	STA=0;  						// Somente se a transmissao for apos um start
	SI=0;   						// Zera bit que sinaliza mudanca de estados
	while(SI==0); 					// Espera realizar a transmissao

	// Verifica status
	if(RW == W)
	{
		if(SMB0STA != 0x18)
		{
			return SMB0STA;
		}
	}
	else
	{
		if(SMB0STA != 0x40)
		{
			return SMB0STA;	// ERRO
		}
	}
 	return 0;
}

unsigned char esc_byte_dado(unsigned char dado)
{
	SMB0DAT = dado; // Transmite o dado

	SI=0; // Zera o bit que sinaliza mudanca de estadosZERA BIT QUE SINALIZA MUDAN A DE ESTATOS
	while(SI==0); // Espera ate realizar a transmissao
	if(SMB0STA != 0x28) return SMB0STA; // Erro

 	return 0;
}

int esc_eeprom(unsigned char end_disp,unsigned char end, unsigned char dado)
{
	unsigned char result;

	result = esc_byte_cntr(end_disp,W);         // Escreve o endereco
	if (result != 0 ) return (-(int)result);

	result = esc_byte_dado(end);                // Escreve o controle
	if (result != 0 ) return (-(int)result);

	result = esc_byte_dado(dado);               // Escreve o dado
	if (result != 0 ) return (-(int)result);

	STO=1; 	// Stop 1 de 3
	SI=0; 	// Stop 2 de 3
	while(STO==1); // Stop 3 de 3

	// A EEPROM demora pra escrever, entao fica verificando se j escreveu,
    // grava de forma n o volatil
	while(esc_byte_cntr(end_disp,W) != 0); // Acknowledge polling.

	return 0;
}

int le_eeprom(unsigned char end_disp, unsigned char end)
{
	unsigned char result;
	int dado;

	result = esc_byte_cntr(end_disp,W);         // Escreve o endereco
	if (result != 0 ) return (-(int)result);

	result = esc_byte_dado(end);                // Escreve o contole
	if (result != 0 ) return (-(int)result);

	result = esc_byte_cntr(end_disp,R);
	if (result != 0 ) return (-(int)result);

    // Recebendo um byte
	AA=0; // Configura MCU para gerar NACK ap s receber um byte
	SI=0;
	while(SI==0);	// Aguarda receber byte da EEPROM

	if(SMB0STA != 0x58) return (-(int)SMB0STA); //verifica status

	dado = (int)SMB0DAT; // L  DO DADO

    // Gerando um stop
	STO=1; 	// Stop 1 de 3
	SI=0; 	// Stop 2 de 3
	while(STO==1); // Stop 3 de 3

	return dado;
}

void Timer4_ISR (void) __interrupt 16
{
	SMB0CN &= ~0x40;// Desabilita SMBus
	SMB0CN |= 0x40;// Habilita SMBus
	TF4 = 0;// Zera flag de interrup  o do TC4
} 

/*********************************************************************
 * 
 * 	DS18B20
 * 
 ********************************************************************/

void Init_DS1820(void)
{
	DQ = 0; // Resetar o pulso
    delay_us(480); // Minimo DE 480us
    DQ = 1;
    delay_us(480); // Apos detectar o pulso de subida espera por no minimo 480us
}

unsigned char ReadOneChar(void)
{
    int i=0;
    unsigned char dat = 0;
    for (i=0;i<8;i++)
    {
		DQ = 0;
		delay_us(1);

		DQ = 1;
		delay_us(1);

		dat>>=1;

		if(DQ) dat|=0x80;

		delay_us(60);
    }
    return(dat);
}

void Write1()
{
	DQ = 0;
	delay_us(1);

	DQ = 1;
	delay_us(60);
}

void Write0()
{
	DQ = 0;
	delay_us(60);

	DQ = 1;
	delay_us(1);
}

void write_bit(unsigned char b)
{
	if(b == 1)
	{
	 	Write1();
	}
	else
	{
	 	Write0();
	}
}

void WriteOneChar(unsigned char b)
{
 	int i;
	for(i = 8 ; i > 0 ; i--)
	{
		write_bit(b%2);
		b >>= 1;
	}
}

float ReadTemperature(void)
{
	unsigned char temp_l,temp_h;
	float temp;

	Init_DS1820();		// Reset
	WriteOneChar(0xcc); // Skip ROM
	WriteOneChar(0x44); // Inicia a conversao de temperatura

	delay_ms(200);  	// Espera ate a conversao acabar

    Init_DS1820(); 		// Reset
	WriteOneChar(0xcc); // Skip ROM
	WriteOneChar(0xbe); // Leitura do registro de temperatura

	temp_l=ReadOneChar(); // Ler o registrador LS
	temp_h=ReadOneChar(); // Ler o registrador HS

	temp = (unsigned int)temp_l / 2;

	if ((unsigned int)temp_h>0)
		temp = -temp;

	return temp;
}

/*********************************************************************
 * 
 * 	FAN PWM
 * 
 ********************************************************************/

// precisa configurar um dos módulos PCA para o modo PWM!

void setPWM(unsigned char dutyCycle)
{
	switch (dutyCycle)
	{
		case 'o': // OFF
			PCA0CPH0 = 255;
			break;

		case 'h': // Half speed
			PCA0CPH0 = 128;
			break;

		case 'f': // Full speed
			PCA0CPH0 = 0;
			break;

		case 'i': // increase speed
			if (PCA0CPH0>10){
				PCA0CPH0-=10;}
			break;	

		case 'd': // decrease speed
			if (PCA0CPH0<255){
				PCA0CPH0+=10;}
			break;

		default: // Default: OFF
			PCA0CPH0 = 255;
			break;
	}
}

void controlFan(float temp, int limite, float histerese)
{
	if (temp > limite + histerese) // Temperatura elevada
	{
		setPWM('f');
	}
	else if (temp < limite - histerese) // Temperatura baixa
	{
		setPWM('o');
	}
	else // Temperatura intermediaria
	{
		setPWM('h');
	}
}
