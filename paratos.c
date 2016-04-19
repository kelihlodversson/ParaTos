#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include "common.h"

#include "m68k.h"
#include "m68kcpu.h"
#include "loader.h"
#include "sysvars.h"

void WriteCookie(uint32_t key, uint32_t value)
{
	uint32_t address = m68k_read_memory_32(_p_cookies);
	int found = 0;
	uint32_t current=0;
	for(;;address += 8)
	{
		current = m68k_read_memory_32(address);
		if (current == 0)
		{
			break;
		}

		if (current == key)
		{
			found = 1;
			break;
		}
	}
	printf("Write %0x @ %0x (%0x)\n",key, address, m68k_read_memory_32(address) );
	m68k_write_memory_32(address, key);
	address += 4;
	printf("Write %0x @ %0x\n",value, address);
	m68k_write_memory_32(address, value);
	address += 4;
	if(!found)
	{
		m68k_write_memory_32(address, 0);
		address += 4;
		m68k_write_memory_32(address, 0);
	}
}

int ReadCookie(uint32_t key, uint32_t* value)
{
	uint32_t address = m68k_read_memory_32(_p_cookies);
	int found = 0;
	if (key > 1 && key <= 0xffff)
	{
		for(int32_t i=1; i <= key; address += 8, i++)
		{
			if (m68k_read_memory_32(address) == 0)
			{
				return 0;
			}
		}
		*value=m68k_read_memory_32(address);
		return 1;
	}
	else
	{
		uint32_t current=0;
		for(;;address += 8)
		{
			current = m68k_read_memory_32(address);
			if (current == 0)
			{
				break;
			}

			if (current == key)
			{
				found = 1;
				break;
			}
		}
	}

	if (found)
		*value=m68k_read_memory_32(address+4);
	return found-1;
}


void InitCookieJar(uint32_t base)
{
	printf("InitCookieJar: %08x\n", base);
	m68k_write_memory_32(_p_cookies, base);
	m68k_write_memory_32(base, 0);
	WriteCookie(0x4d694e54, 0x110); // MiNT
	WriteCookie(0x5f435055, 20); // _CPU => 20
	WriteCookie(0x5f465055, 0); // _FPU => none
}

void dispatch_gem_trap()
{
	uint32_t pc = m68k_get_reg(NULL, M68K_REG_PC);
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
    uint16_t num = m68k_read_memory_16(sp);
	uint32_t ad0 = m68k_get_reg(NULL, M68K_REG_D0);
    uint32_t ad1 = m68k_get_reg(NULL, M68K_REG_D1);

    //printf("GEM\n");

    printf("AES sp=0x%08x pc=0x%08x\n", sp, pc);

    m68k_set_reg(M68K_REG_D0, -(int)num);
}

void dispatch_bios_trap()
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
    uint16_t num = m68k_read_memory_16(sp);

    printf("BIOS(0x%02x)\n", num);

    m68k_set_reg(M68K_REG_D0, -(int)num);
}


void dispatch_xbios_trap()
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
    int16_t num = m68k_read_memory_16(sp);
	int16_t retval = -num;
	switch(num)
	{
		case 4: // getrez
		retval = 3; // ST Mono
		break;
		case 17: // random
		retval = rand() & 0x00ffffff;
		break;
		case 38: // Supexec
		// We're just going to fake it an run it in user mode anyway
		{
			uint32_t call = m68k_read_memory_32(sp+2);
			uint32_t callee = m68k_get_reg(NULL, M68K_REG_PC);
			m68k_write_memory_32(sp-4, callee);
			m68k_set_reg(M68K_REG_SP, sp-4);
			m68k_set_reg(M68K_REG_PC, call);
			return;
		}
		default:
		printf("XBIOS(0x%02x)\n", num);
	}

    m68k_set_reg(M68K_REG_D0, retval);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <program.tos> [arguments...]\n", argv[0]);
        return 1;
    }

	memory_sz = 16 * 1024 * 1024;
	memory = calloc(1, memory_sz);// 16 Mb memory

	InitCookieJar(0x1000);
	basepage_t* bp = LoadExe(argv[1], argv+2, argc-2);
    if (bp == 0)
    {
        fprintf(stderr, "error: cannot load %s.\n", argv[1]);
        return 1;
    }

	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68k_pulse_reset();

	uint32_t stack = bp->p_hitpa;
	uint32_t base = (uint32_t)((uint64_t)bp-(uint64_t)memory);

	stack-=4;
	m68k_write_memory_32(stack, base);
	stack-=4;
	m68k_write_memory_32(stack, 0);

    m68k_set_reg(M68K_REG_SP, (uint32_t)(((uint64_t)bp-4)-(uint64_t)memory));
    m68k_set_reg(M68K_REG_SR, 0x0300);
    m68k_set_reg(M68K_REG_SP, stack);
    m68k_set_reg(M68K_REG_PC, bp->p_tbase);

    for (;;)
    {
        m68k_execute(10000);
    }

    return 0;
}