/* Copyright 2020, Franco Bucafusco
 * All rights reserved.
 *
 * This file is part of sAPI Library.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*=====[Inclusions of function dependencies]=================================*/
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "sapi.h"
#include "keys.h"

/*=====[Definition & macros of public constants]==============================*/

/*=====[Definitions of extern global functions]==============================*/

// Prototipo de funcion de la tarea
void task_led( void* taskParmPtr );
void task_tecla( void* taskParmPtr );

typedef struct
{
    gpioMap_t led;
    xQueueHandle queue; //almacenara el evento en una cola
} t_tecla_led;


t_tecla_led leds_data[] = { {.led= LEDR}, {.led= LED1}, {.led= LED2}, {.led= LED3}};

/*=====[Definitions of public global variables]==============================*/

/*=====[Main function, program entry point after power on or reset]==========*/

int main( void )
{
    BaseType_t res;

    // ---------- CONFIGURACIONES ------------------------------
    boardConfig();  // Inicializar y configurar la plataforma

    printf( "Ejercicio F3\n" );

    for( int i = 0; i < 4; i++ )
    {
        // Crear tareas en freeRTOS
        res = xTaskCreate (
                  task_led,					// Funcion de la tarea a ejecutar
                  ( const char * )"task_led",	// Nombre de la tarea como String amigable para el usuario
                  configMINIMAL_STACK_SIZE*2,	// Cantidad de stack de la tarea
                  &leds_data[i],							// Parametros de tarea
                  tskIDLE_PRIORITY+1,			// Prioridad de la tarea
                  0							// Puntero a la tarea creada en el sistema
              );

        // Gestión de errores
        configASSERT( res == pdPASS );

    }

    /* inicializo driver de teclas */
    keys_Init();

    // Iniciar scheduler
    vTaskStartScheduler();					// Enciende tick | Crea idle y pone en ready | Evalua las tareas creadas | Prioridad mas alta pasa a running

    /* realizar un assert con "false" es equivalente al while(1) */
    configASSERT( 0 );
    return 0;
}
uint32_t estado=0;
gpioMap_t primera_tecla;
gpioMap_t segunda_tecla;
TickType_t primera_tecla_time;
TickType_t segunda_tecla_time;


// Estructura principal
typedef struct
{
	gpioMap_t tecla;
	TickType_t tiempo_medido;
	fsmButtonState_t fsmButtonState;

} mjs_queque;

void mv(){
	mjs_queque* booton_1;
	mjs_queque* booton_2;

	if(xQueueMessagesWaiting(isr_queue)==2){
		xQueueReceive( isr_queue, &booton_1, 0 );
		xQueueReceive( isr_queue, &booton_2, 0 );

		 if((booton_1->tecla=!booton_2->tecla)){
			 if(booton_2->fsmButtonState==STATE_BUTTON_DOWN && booton_1->fsmButtonState==STATE_BUTTON_DOWN){
				 send_mjs_uart(booton_1,booton_2);

			 }else{
				 send_mjs_uart(booton_1,booton_2);

			 }
		 }
	}
}






void mv( t_key_isr_signal* event_data ){
	switch(estado){

	    case 0: //aca ninguna tecla presionada
	    	if(even_data->event_type==TEC_FAIL){
				primera_tecla=even_data->tecla;
				primera_tecla_time=even_data->even_time;
				estado=1;
	    	}

	    	break;
	    case 1:  //aca una tecla presionada
	    	if(even_data->event_type==TEC_FAIL){
	    			segunda_tecla=even_data->tecla;
	    			segunda_tecla_time=even_data->even_time;
	    			//hacer acala dif entre primera_tecla_time y segunda_tecla_time y mandar por uart
	    			estado=2;
	    	}else{
	    		estado=0;
	    		/*flanco ascendente*/
	    	}
	    	break;
	    case 2: //aca dos teclas presiondas
	       	if(even_data->event_type==TEC_RISE){
	    			primera_tecla=even_data->tecla;
	    			primera_tecla_time=even_data->even_time;
	    			estado=3;
	        	}
	    	break;
	    case 3:
	    	if(even_data->event_type==TEC_RISE){
	    		segunda_tecla=even_data->tecla;
	    		segunda_tecla_time=even_data->even_time;
	    		//hacer acala dif entre primera_tecla_time y segunda_tecla_time y mandar por uart
	    		estado=0;
	    	}else{
	    		estado=0;
	    		/*flanco descendente*/
	    	}
	    	break;
	    }
}
void user_buttonPressed( t_key_isr_signal* event_data )
{
    mv(event_data);


}

void user_buttonReleased( t_key_isr_signal* event_data )
{
	   mv(event_data);
}


void task_led( void* taskParmPtr )
{
    t_key_isr_signal evnt;
    t_tecla_led* led_data = taskParmPtr;

    led_data->queue = xQueueCreate( 2, sizeof(   t_key_isr_signal )  );

    TickType_t dif =   pdMS_TO_TICKS( 500 );
    int tecla_presionada;
    TickType_t xPeriodicity = pdMS_TO_TICKS( 1000 ); // Tarea periodica cada 1000 ms

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while( 1 )
    {
        if( xQueueReceive( led_data->queue, &evnt, 0 ) == pdPASS  )
        {
            dif = get_diff( evnt.tecla );
        }

        gpioWrite( led_data->led, ON );
        vTaskDelay( dif );
        gpioWrite( led_data->led, OFF );


        // Envia la tarea al estado bloqueado durante xPeriodicity (delay periodico)
        vTaskDelayUntil( &xLastWakeTime, 2*dif );
    }
}

/* hook que se ejecuta si al necesitar un objeto dinamico, no hay memoria disponible */
void vApplicationMallocFailedHook()
{
    printf( "Malloc Failed Hook!\n" );
    configASSERT( 0 );
}
