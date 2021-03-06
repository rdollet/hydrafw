/*
HydraBus/HydraNFC - Copyright (C) 2014 Benjamin VERNOUX

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "hydrabus_mode_uart.h"
#include "xatoi.h"
#include "bsp_uart.h"
#include <string.h>

static const char* str_pins_uart1= { "UART1 TX=PA9, RX=PA10" };
static const char* str_pins_uart2= { "UART2 TX=PA2, RX=PA3" };
static const char* str_name_uart= { "UART" };
static const char* str_prompt_uart1= { "uart1> " };
static const char* str_prompt_uart2= { "uart2> " };

const mode_exec_t mode_uart_exec = {
	.mode_cmd          = &mode_cmd_uart,       /* Terminal parameters specific to this mode */
	.mode_start        = &mode_start_uart,     /* Start command '[' */
	.mode_startR       = &mode_startR_uart,    /* Start Read command '{' */
	.mode_stop         = &mode_stop_uart,      /* Stop command ']' */
	.mode_stopR        = &mode_stopR_uart,     /* Stop Read command '}' */
	.mode_write        = &mode_write_uart,     /* Write/Send 1 data */
	.mode_read         = &mode_read_uart,      /* Read 1 data command 'r' */
	.mode_write_read   = &mode_write_read_uart,/* Write & Read 1 data implicitely with mode_write command */
	.mode_clkh         = &mode_clkh_uart,      /* Set CLK High (x-WIRE or other raw mode ...) command '/' */
	.mode_clkl         = &mode_clkl_uart,      /* Set CLK Low (x-WIRE or other raw mode ...) command '\' */
	.mode_dath         = &mode_dath_uart,      /* Set DAT High (x-WIRE or other raw mode ...) command '-' */
	.mode_datl         = &mode_datl_uart,      /* Set DAT Low (x-WIRE or other raw mode ...) command '_' */
	.mode_dats         = &mode_dats_uart,      /* Read Bit (x-WIRE or other raw mode ...) command '!' */
	.mode_clk          = &mode_clk_uart,       /* CLK Tick (x-WIRE or other raw mode ...) command '^' */
	.mode_bitr         = &mode_bitr_uart,      /* DAT Read (x-WIRE or other raw mode ...) command '.' */
	.mode_periodic     = &mode_periodic_uart,  /* Periodic service called (like UART sniffer...) */
	.mode_macro        = &mode_macro_uart,     /* Macro command "(x)", "(0)" List current macros */
	.mode_setup        = &mode_setup_uart,     /* Configure the device internal params with user parameters (before Power On) */
	.mode_setup_exc    = &mode_setup_exc_uart, /* Configure the physical device after Power On (command 'W') */
	.mode_cleanup      = &mode_cleanup_uart,   /* Exit mode, disable device enter safe mode UART... */
	.mode_print_param    = &mode_print_param_uart,    /* Print Mode parameters */
	.mode_print_pins     = &mode_print_pins_uart,     /* Print Pins used */
	.mode_print_settings = &mode_print_settings_uart, /* Settings string */
	.mode_print_name     = &mode_print_name_uart,      /* Print Mode name */
	.mode_str_prompt   = &mode_str_prompt_uart    /* Prompt name string */
};

static const char* str_dev_arg_num[]= {
	"Choose UART device number: 1=UART1, 2=UART2\r\n"
};
static const char* str_dev_param_num[]= {
	"1=UART1",
	"2=UART2"
};

static const char* str_dev_arg_speed[]= {
	"Choose UART Freq:\r\n1=300bps, 2=1200bps, 3=2400bps, 4=4800bps, 5=9600bps\r\n6=19200bps,7=38400bps,8=57600bps,9=115200bps, 10=31250bps\r\nmanual up to 10.5mbps\r\n"
};
static const char* str_dev_param_speed[]= {
	/* UART1, 2 */
	/* 0  */ "1=300bps",
	/* 1  */ "2=1200bps",
	/* 2  */ "3=2400bps",
	/* 3  */ "4=4800bps",
	/* 4  */ "5=9600bps",
	/* 5  */ "6=19200bps",
	/* 6  */ "7=38400bps",
	/* 7  */ "8=57600bps",
	/* 8  */ "9=115200bps",
	/* 9  */ "10=31250bps"
	/* 10 */ "manual up to 10.5mbps"
};

static const char* str_dev_arg_parity[]= {
	"Choose UART Parity: 1=8/none, 2=8/even, 3=8/odd\r\n"
};
static const char* str_dev_param_parity[]= {
	"1=8/none",
	"2=8/even",
	"3=8/odd"
};

static const char* str_dev_arg_stop_bit[]= {
	"Choose UART Nb Stop Bit: 1=1 stop, 2=2 stop\r\n"
};
static const char* str_dev_param_stop_bit[]= {
	"1=1 stop",
	"2=2 stop"
};

static const mode_dev_arg_t mode_dev_arg[] = {
	/* argv0 */ { .min=1, .max=2, .dec_val=TRUE, .param=DEV_NUM, .argc_help=ARRAY_SIZE(str_dev_arg_num), .argv_help=str_dev_arg_num },
	/* argv1 */ { .min=1, .max=10500000, .dec_val=TRUE, .param=DEV_SPEED, .argc_help=ARRAY_SIZE(str_dev_arg_speed), .argv_help=str_dev_arg_speed },
	/* argv2 */ { .min=1, .max=3, .dec_val=TRUE, .param=DEV_PARITY, .argc_help=ARRAY_SIZE(str_dev_arg_parity), .argv_help=str_dev_arg_parity },
	/* argv3 */ { .min=1, .max=2, .dec_val=TRUE, .param=DEV_STOP_BIT, .argc_help=ARRAY_SIZE(str_dev_arg_stop_bit), .argv_help=str_dev_arg_stop_bit },
};
#define MODE_DEV_NB_ARGC ((int)ARRAY_SIZE(mode_dev_arg)) /* Number of arguments/parameters for this mode */

/* Terminal parameters management specific to this mode */
/* Return TRUE if success else FALSE */
bool mode_cmd_uart(t_hydra_console *con, int argc, const char* const* argv)
{
	long dev_val;
	int arg_no;

	if(argc == 0) {
		hydrabus_mode_dev_manage_arg(con, 0, NULL, 0, 0, (mode_dev_arg_t*)&mode_dev_arg);
		return FALSE;
	}

	/* Ignore additional parameters */
	if(argc > MODE_DEV_NB_ARGC)
		argc = MODE_DEV_NB_ARGC;

	for(arg_no = 0; arg_no < argc; arg_no++) {
		dev_val = hydrabus_mode_dev_manage_arg(con, argc, argv, MODE_DEV_NB_ARGC, arg_no, (mode_dev_arg_t*)&mode_dev_arg);
		if(dev_val == HYDRABUS_MODE_DEV_INVALID) {
			return FALSE;
		}
	}

	if(argc == MODE_DEV_NB_ARGC) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/* Start command '[' */
void mode_start_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* Start Read command '{' */
void mode_startR_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* Stop command ']' */
void mode_stop_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* Stop Read command '}' */
void mode_stopR_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* Write/Send x data return status 0=OK */
uint32_t mode_write_uart(t_hydra_console *con, uint8_t *tx_data, uint8_t nb_data)
{
	int i;
	uint32_t status;
	mode_config_proto_t* proto = &con->mode->proto;

	status = bsp_uart_write_u8(proto->dev_num, tx_data, nb_data);
	if(status == BSP_OK) {
		if(nb_data == 1) {
			/* Write 1 data */
			cprintf(con, hydrabus_mode_str_write_one_u8, tx_data[0]);
		} else if(nb_data > 1) {
			/* Write n data */
			cprintf(con, hydrabus_mode_str_mul_write);
			for(i = 0; i < nb_data; i++) {
				cprintf(con, hydrabus_mode_str_mul_value_u8, tx_data[i]);
			}
			cprintf(con, hydrabus_mode_str_mul_br);
		}
	}
	return status;
}

/* Read x data command 'r' return status 0=OK */
uint32_t mode_read_uart(t_hydra_console *con, uint8_t *rx_data, uint8_t nb_data)
{
	int i;
	uint32_t status;
	mode_config_proto_t* proto = &con->mode->proto;

	status = bsp_uart_read_u8(proto->dev_num, rx_data, nb_data);
	if(status == BSP_OK) {
		if(nb_data == 1) {
			/* Read 1 data */
			cprintf(con, hydrabus_mode_str_read_one_u8, rx_data[0]);
		} else if(nb_data > 1) {
			/* Read n data */
			cprintf(con, hydrabus_mode_str_mul_read);
			for(i = 0; i < nb_data; i++) {
				cprintf(con, hydrabus_mode_str_mul_value_u8, rx_data[i]);
			}
			cprintf(con, hydrabus_mode_str_mul_br);
		}
	}
	return status;
}

/* Write & Read x data return status 0=OK */
uint32_t mode_write_read_uart(t_hydra_console *con, uint8_t *tx_data, uint8_t *rx_data, uint8_t nb_data)
{
	int i;
	uint32_t status;
	mode_config_proto_t* proto = &con->mode->proto;

	status = bsp_uart_write_read_u8(proto->dev_num, tx_data, rx_data, nb_data);
	if(status == BSP_OK) {
		if(nb_data == 1) {
			/* Write & Read 1 data */
			cprintf(con, hydrabus_mode_str_write_read_u8, tx_data[0], rx_data[0]);
		} else if(nb_data > 1) {
			/* Write & Read n data */
			for(i = 0; i < nb_data; i++) {
				cprintf(con, hydrabus_mode_str_write_read_u8, tx_data[i], rx_data[i]);
			}
		}
	}
	return status;
}

/* Set CLK High (x-WIRE or other raw mode ...) command '/' */
void mode_clkh_uart(t_hydra_console *con)
{
	(void)con;

	/* Nothing to do in UART mode */
}

/* Set CLK Low (x-WIRE or other raw mode ...) command '\' */
void mode_clkl_uart(t_hydra_console *con)
{
	(void)con;

	/* Nothing to do in UART mode */
}

/* Set DAT High (x-WIRE or other raw mode ...) command '-' */
void mode_dath_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* Set DAT Low (x-WIRE or other raw mode ...) command '_' */
void mode_datl_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* Read Bit (x-WIRE or other raw mode ...) command '!' */
void mode_dats_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* CLK Tick (x-WIRE or other raw mode ...) command '^' */
void mode_clk_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* DAT Read (x-WIRE or other raw mode ...) command '.' */
void mode_bitr_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* Periodic service called (like UART sniffer...) */
uint32_t mode_periodic_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
	return 0;
}

/* Macro command "(x)", "(0)" List current macros */
void mode_macro_uart(t_hydra_console *con, uint32_t macro_num)
{
	(void)con;
	(void)macro_num;
	/* TODO mode_uart Macro command "(x)" */
}

/* Configure the device internal params with user parameters (before Power On) */
void mode_setup_uart(t_hydra_console *con)
{
	(void)con;
	/* Nothing to do in UART mode */
}

/* Configure the physical device after Power On (command 'W') */
void mode_setup_exc_uart(t_hydra_console *con)
{
	mode_config_proto_t* proto = &con->mode->proto;

	bsp_uart_init(proto->dev_num, proto);
}

/* Exit mode, disable device safe mode UART... */
void mode_cleanup_uart(t_hydra_console *con)
{
	mode_config_proto_t* proto = &con->mode->proto;

	bsp_uart_deinit(proto->dev_num);
}

/* Mode parameters string (does not include m & bus_mode) */
void mode_print_param_uart(t_hydra_console *con)
{

	cprintf(con, "%d %d %d %d",
		con->mode->proto.dev_num+1, con->mode->proto.dev_speed+1,
		con->mode->proto.dev_parity+1, con->mode->proto.dev_stop_bit+1);
}

/* Print pins used */
void mode_print_pins_uart(t_hydra_console *con)
{
	mode_config_proto_t* proto = &con->mode->proto;

	if(proto->dev_num == 0)
		cprint(con, str_pins_uart1, strlen(str_pins_uart1));
	else
		cprint(con, str_pins_uart2, strlen(str_pins_uart2));
}

static void print_speed(t_hydra_console *con)
{
	mode_config_proto_t* proto = &con->mode->proto;

	if(proto->dev_speed < 10) {
		cprintf(con, "%s", str_dev_param_speed[proto->dev_speed]);
	} else {
		cprintf(con, "%dbps", proto->dev_speed+1);
	}
}

/* Print settings */
void mode_print_settings_uart(t_hydra_console *con)
{
	mode_config_proto_t* proto = &con->mode->proto;

	cprintf(con, "Device: %s\r\nSpeed: ", str_dev_param_num[proto->dev_num]);
	print_speed(con);
	cprintf(con, "\r\nParity: %s\r\nNb Stop Bit: %s",
		str_dev_param_parity[proto->dev_parity],
		str_dev_param_stop_bit[proto->dev_stop_bit]);
}

/* Print mode name */
void mode_print_name_uart(t_hydra_console *con)
{
	cprint(con, str_name_uart, strlen(str_name_uart));
}

/* Return Prompt name */
const char* mode_str_prompt_uart(t_hydra_console *con)
{
	mode_config_proto_t* proto = &con->mode->proto;

	if( proto->dev_num == 0) {
		return str_prompt_uart1;
	} else
		return str_prompt_uart2;
}
