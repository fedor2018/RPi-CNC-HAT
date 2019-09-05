-- Copyright (C) 1991-2013 Altera Corporation
-- Your use of Altera Corporation's design tools, logic functions 
-- and other software and tools, and its AMPP partner logic 
-- functions, and any output files from any of the foregoing 
-- (including device programming or simulation files), and any 
-- associated documentation or information are expressly subject 
-- to the terms and conditions of the Altera Program License 
-- Subscription Agreement, Altera MegaCore Function License 
-- Agreement, or other applicable license agreement, including, 
-- without limitation, that your use is for the sole purpose of 
-- programming logic devices manufactured by Altera and sold by 
-- Altera or its authorized distributors.  Please refer to the 
-- applicable agreement for further details.

-- ***************************************************************************
-- This file contains a Vhdl test bench template that is freely editable to   
-- suit user's needs .Comments are provided in each section to help the user  
-- fill out necessary details.                                                
-- ***************************************************************************
-- Generated on "01/11/2019 15:16:35"
                                                            
-- Vhdl Test Bench template for design  :  spi_main
-- 
-- Simulation tool : ModelSim-Altera (VHDL)
-- 

LIBRARY ieee;                                               
USE ieee.std_logic_1164.all;                                

ENTITY spi_main_vhd_tst IS
END spi_main_vhd_tst;
ARCHITECTURE spi_main_arch OF spi_main_vhd_tst IS
-- constants                                                 
-- signals                                                   
SIGNAL LED : STD_LOGIC;
SIGNAL MISO : STD_LOGIC;
SIGNAL MOSI : STD_LOGIC;
SIGNAL SCK : STD_LOGIC;
SIGNAL SSEL : STD_LOGIC;
SIGNAL clk : STD_LOGIC;
SIGNAL din : STD_LOGIC_VECTOR(15 DOWNTO 0);
SIGNAL dir : STD_LOGIC_VECTOR(3 DOWNTO 0);
SIGNAL dout : STD_LOGIC_VECTOR(13 DOWNTO 0);
SIGNAL step : STD_LOGIC_VECTOR(3 DOWNTO 0);
COMPONENT spi_main
	PORT (
	LED : OUT STD_LOGIC;
	MISO : OUT STD_LOGIC;
	MOSI : IN STD_LOGIC;
	SCK : IN STD_LOGIC;
	SSEL : IN STD_LOGIC;
	clk : IN STD_LOGIC;
	din : IN STD_LOGIC_VECTOR(15 DOWNTO 0);
	dir : OUT STD_LOGIC_VECTOR(3 DOWNTO 0);
	dout : OUT STD_LOGIC_VECTOR(13 DOWNTO 0);
	step : OUT STD_LOGIC_VECTOR(3 DOWNTO 0)
	);
END COMPONENT;
BEGIN
	i1 : spi_main
	PORT MAP (
-- list connections between master ports and signals
	LED => LED,
	MISO => MISO,
	MOSI => MOSI,
	SCK => SCK,
	SSEL => SSEL,
	clk => clk,
	din => din,
	dir => dir,
	dout => dout,
	step => step
	);
init : PROCESS                                               
-- variable declarations                                     
BEGIN                                                        
        -- code that executes only once                      
WAIT;                                                       
END PROCESS init;                                           
always : PROCESS                                              
-- optional sensitivity list                                  
-- (        )                                                 
-- variable declarations                                      
BEGIN                                                         
        -- code executes for every event on sensitivity list  
WAIT;                                                        
END PROCESS always;                                          
END spi_main_arch;
