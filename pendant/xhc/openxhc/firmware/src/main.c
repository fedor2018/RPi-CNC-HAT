/*********************************************************************\
* Copyleft (>) 2014 Roman 'Fallout' Ilichev <fxdteam@gmail.com>      *
 *                                                                   *
 * This file is part of OpenXHC project                              *
 *                             WTFPL LICENSE v2                      *
\*********************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"

#include "xhc_dev.h"
#include "lcd_driver.h"
#include "kbd_driver.h"
#include "io_input.h"
#include "timer_drv.h"

__IO uint8_t PrevXferComplete = 1;
__IO uint8_t g_hw_type = DEV_WHB04;
/* used as key for XOR */
__IO uint8_t day = 0;
int16_t encoder_val = 0;
uint8_t rotary_pos = 0;
uint8_t switch_old_val = 0;

int g_render_lcd = 0;

struct whb04_out_data output_report = { 0 };
struct whb0x_in_data in_report = { .id = 0x04 };



/* EP1 packet goes out OK */
void EP1_IN_Callback(void)
{
  PrevXferComplete = 1; 
}

void xhc_send( void )
{
  kbd_driver.read( &in_report.btn_1, &in_report.btn_2 );
  
  in_report.xor_day = day ^ in_report.btn_1;
  in_report.wheel = encoder_val;
  in_report.wheel_mode = rotary_pos;
    
  /* Reset the control token to inform upper layer that a transfer is ongoing */
  PrevXferComplete = 0;
  
  /* copy buffer in ENDP1 Tx Packet Memory Area*/
  USB_SIL_Write( EP1_IN, (void*)&in_report, sizeof ( struct whb0x_in_data )  );
  
  /* enable endpoint for transmission */
  SetEPTxValid(ENDP1);
}

/* recive state machine */
void xhc_recv( uint8_t *data )
{
#define TMP_BUFF_SIZE   42
#define CHUNK_SIZE      7
  static int offset = 0;
  static uint8_t magic_found = 0;
  static uint8_t tmp_buff[TMP_BUFF_SIZE];
  
  /* check for magic */
  if( *(uint16_t*)data == WHBxx_MAGIC )
  {
      offset = 0;
      magic_found = 1;
  }
  
  if( !magic_found )
    return;
  
  if( (offset+CHUNK_SIZE) > TMP_BUFF_SIZE )
    return;
  memcpy(  &tmp_buff[offset], data, CHUNK_SIZE );
  offset+=CHUNK_SIZE;
  
  /* we recive all data ? */
  if( offset < g_hw_type )
    return;

  magic_found = 0;
  /* convert data to whb04 format */
  if( g_hw_type == DEV_WHB03 )
  {
    struct whb03_out_data* p = (struct whb03_out_data*)tmp_buff;
    int i;
    
    output_report.magic = p->magic;
    output_report.day = p->day;
    output_report.feedrate_ovr = p->feedrate_ovr;
    output_report.sspeed_ovr = p->sspeed_ovr;
    output_report.feedrate = p->feedrate;
    output_report.sspeed = p->sspeed;
    output_report.step_mul = p->step_mul;
    output_report.state = p->state;
    
    for( i = 0; i < 6; i++ )
    {
      uint16_t tmp = (p->pos[i].p_frac & 0x80)<<8;
      p->pos[i].p_frac &=~0x80;
      tmp |= p->pos[i].p_frac * 100l; 
      output_report.pos[i].p_int = p->pos[i].p_int;
      output_report.pos[i].p_frac = tmp;
    }
  }
  else
  {
    output_report = *((struct whb04_out_data*)tmp_buff);
  }
  /* update XOR key */
  day = output_report.day;
  g_render_lcd = 1;
}

int main(void)
{
  int state_changed = 0;
  
  Set_System();
  /* init encoder, btns, switch etc */
  io_driver.init();
  /* used for delays */
  timer_driver.init();
  g_hw_type = io_driver.hw_is_xhb04() ? DEV_WHB04:DEV_WHB03;
  
  /* init usb stuff */
  USB_Interrupts_Config();
  Set_USBClock();
  USB_Init();
  
  /* init matrix keyboard */
  kbd_driver.init();
  
  /* init lcd hardware */
  lcd_driver.init();
  lcd_driver.clear_screen();
  
  lcd_driver.draw_text( (g_hw_type == DEV_WHB03) ? "XHC HB03":"XHC HB04", 0, 1 );
   
  switch_old_val = io_driver.pos_is_wc();
  io_poll_tmr = 500;
  for (;;)
  {
    static uint8_t nkeys = 0, c1, c2;
    static int16_t encoder = 0;
    
    if (bDeviceState != CONFIGURED )
      continue;
    
    if( g_render_lcd )
    {
      --g_render_lcd;
      lcd_driver.render_screen( &output_report, rotary_pos, io_driver.pos_is_wc() );
    }
    
    /* update input events state */
    if( io_poll_tmr )
      continue;

    uint8_t tmp;
      
    tmp = io_driver.rotary_read();
    if( rotary_pos != tmp )
    {
      g_render_lcd = 1;
      state_changed |= 2;
      rotary_pos = tmp;
    }
      
    if( switch_old_val != io_driver.pos_is_wc() )
    {
      switch_old_val = io_driver.pos_is_wc();
      g_render_lcd = 1;
    }
      
    encoder_val = io_driver.encoder_read();
    if( rotary_pos && (encoder_val || (encoder_val != encoder )) )
    {
      if( encoder_val > 0 )
        encoder_val = 1;
      else if( encoder_val < 0 )
        encoder_val = -1;
      state_changed |= 1;
      encoder = encoder_val;
    }
      
    uint8_t k1, k2;
    do
    {
      k1 = c1, k2 = c2;
      tmp = kbd_driver.read( &c1, &c2 );
    }
    while( (k1!=c1) || (k2 != c2 ) );
    if( nkeys != tmp )
    {
      state_changed |=4;
      nkeys = tmp;
    }
    
    io_poll_tmr = 30;

    if( state_changed )
    {   
       xhc_send();
       state_changed = 0;
    }
  }
}



