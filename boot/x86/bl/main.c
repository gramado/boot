/*
 * BL.BIN
 * Gramado Boot Loader
 * (c) Copyright 2015-2020 Fred Nora.
 * File: main.c 
 * + It loads the kernel image.
 * + ... initializes the paging.
 * + passes the command to the kernel.
 * The boot loader was loaded in the address 0x00020000, and
 * has the entry point in the address 0x00021000.
 * The kernel image will be loaded in the address 0x00100000
 * and  the entry point will be in the address 0x00101000.
 * The virtual addresses are 0xC0000000 for the base and
 * 0xC0001000 for the entry point.
 * History:
 *     2015 - Created by Fred Nora.
 *     2020 - Revision.
 */

#include <bootloader.h>

const char *image_name = "KERNEL.BIN";

//
// globals
//

struct display_device_d  DisplayDevice;

int ____testing_memory_size_flag=0;
unsigned long __last_valid_address=0;

// main flags.
int gdefLegacyBIOSBoot=0;
int gdefEFIBoot=0;
int gdefSafeBoot=0;
int gdefShowLogo=0;
int gdefShowProgressBar=0;
//...

// See: gdef.h
struct boot_block_d  BootBlock;

int current_mode=0;

int g_initialized=0;
int g_driver_hdd_initialized=0;


unsigned long g_cursor_x=0;        //Cursor
unsigned long g_cursor_y=0;        //Cursor
unsigned long g_system_color=0;    //Color 
unsigned long g_char_attrib=0;     //Color  
unsigned long CURSOR_X=0;
unsigned long CURSOR_Y=0;
unsigned long STATUS_X=0;
unsigned long STATUS_Y=0;
unsigned long EDITBOX_X=0;
unsigned long EDITBOX_Y=0;
unsigned long EDITBOX_LARGURA=0;
unsigned long EDITBOX_ALTURA=0;
unsigned long EDITBOX_BG_COLOR=0;
unsigned long EDITBOX_TEXT_COLOR=0;

// cr3 support
unsigned long LegacyCR3=0;        //Valor existente.
unsigned long BootLoaderCR3=0;    //Valor configurado pelo Boot Loader.

int useGUI=0;

unsigned long g_proc_status=0;
unsigned long g_next_app=0;
unsigned long g_next_proc=0;
unsigned long g_nova_mensagem=0;
unsigned long next_address=0;    //??


unsigned long g_lbf_pa=0;    //Endereço Físico do LFB.
unsigned long g_lbf_va=0;    //Endereço Lógico do LFB.

// See: bootloader.h
system_t system_info;
VideoBlock_t VideoBlock;    //@todo: Usar um ponteiro.
vesa_t vesa_mode;
procedure_t procedure_info;

// menu
// see: bootloader.h
int menu_highlight=0;
struct menuitem_d MENU[8];

// see: bli386.h
int bliTesting=0;

//static char *codename = "Gramado Boot";
//char kernel_file_name[] = "kernel.bin";
//static char **argv = { NULL, NULL, NULL };
//static char **envp = { NULL, NULL, NULL };

//
// == Private functions: Prototypes =======================
//

static unsigned long init_testing_memory_size(int mb);
static unsigned long __setup_physical_memory(void);
static void 
__clean_memory( 
    unsigned long start_address, 
    unsigned long end_address );
static int blLoadKernelImage(void);
static void blShowMenu(void);
static void blMenu(void);

static void __no_x86_64_was_found(void);
// =========================================

//================================================================
// begin - Testing memory size
//================================================================

static unsigned long init_testing_memory_size(int mb)
{
    unsigned char *BASE = (unsigned char *) 0;
    int offset=0; 
    register int i=0;
// Salvando os valores durante o test.
    unsigned char ____value1 = 0;
    unsigned char ____value2 = 0;
//
// Flag
//
    // Acionando flag.
    ____testing_memory_size_flag = 1;

//#debug
/*
    printf ("=========================================\n");
    printf ("init_testing_memory_size: Looking for %d MB base...\n", mb);
    refresh_screen();
*/

// Começamos em 1MB porque o primeiro mega contem coisa do bios.
    for (i=1; i< (mb+1); i++)
    {
        //printf ("i=%d \n", i);
        //refresh_screen();

        offset = 0x00100000 * i;
        
        //printf ("coloca \n");
        //refresh_screen();

         // #bugbug
         // cuidado com ponteiro nulo.

        //coloca.
        BASE[offset +0] = 0xAA;  //1
        BASE[offset +1] = 0x55;  //2
        
        //printf ("retira \n");
        //refresh_screen();

        //retira dois chars.
        ____value1 = BASE[offset +0];
        ____value2 = BASE[offset +1];
        
        // Se retiramos os mesmos bytes que colocamos.
        if (____value1 == 0xAA && ____value2 == 0x55)
        {
            // Salvamos o ultimo endereco de memoria valido.
            __last_valid_address = (unsigned long) &BASE[offset];
        
            // continua sondando.

        // Se nao conseguimos retirar os mesmos bytes que colocamos
        // e nao tivemos uma excecao.
        }else{

            ____testing_memory_size_flag = 0;
            
            printf("__testing_memory_size: Out of range with no exception!\n");
            printf("__testing_memory_size: Last valid address = %x\n", 
                __last_valid_address);
            refresh_screen();

            ____testing_memory_size_flag = 0;
            return (unsigned long) __last_valid_address;
            
            /*
            while (1){
                asm ("cli");
                asm ("hlt");
            }
            */
        }
    };

     ____testing_memory_size_flag = 0;
            
// Ok, temos um endereço de memoria
// tambem sera salvo em uma variavel global 
// para o caso de panic.

    return (unsigned long) __last_valid_address;
}

//================================================================
// end - Testing memory size
//================================================================

// OUT: Return last valid address.
static unsigned long __setup_physical_memory(void)
{
//++
// == Memory size ===============================
// #test
// Sondando quanta memoria fisica tem instalada.
// Faremos isso antes de mapearmos qualquer coisa e
// antes de carregarmos qualquer arquivo.
// #todo:
// Temos varias questoes a se considerar, como o fato de sujarmos
// a IVT no inicio da memoria.

    unsigned long __address = 0;

    // ok
    //__address = (unsigned long) init_testing_memory_size (4);
    // ok
    //__address = (unsigned long) init_testing_memory_size (64);
    // ok
    //__address = (unsigned long) init_testing_memory_size (127);  
    // 511
    //__address = (unsigned long) init_testing_memory_size (1024);   

    //para testar na maquina real com 2048 mb instalado.
    __address = (unsigned long) init_testing_memory_size(2048);   
    
// #todo
// Temos que passar esses valores para o kernel,
// juntamente com os valores que passsamos durante a inicialização
// em assembly, através do boot buffer em 0x0000000000090000. 
// See: head.s
// LFB_VA:      0x00090000 + 0
// WIDTH:       0x00090000 + 8
// HEIGHT:      0x00090000 + 16
// BPP:         0x00090000 + 24
// LAST_VALID:  0x00090000 + 32  <<<--------
// LAST_VALID:  0x00090000 + 40 (complement)
// ...

    unsigned long *LastValid = 
        (unsigned long*) (0x00090000 + 32); 
    
    unsigned long *LastValidComplement = 
        (unsigned long*) (0x00090000 + 36); 

    LastValid[0]           = (unsigned long) __address;
    LastValidComplement[0] = (unsigned long) 0;

//#debug
    //printf ("BlMain: Las valid PA = %x \n", __address);
    //refresh_screen();
    //while(1){}
// =======================================
//--

// Return last valid address.
    return (unsigned long) __address;
}

// #bugbug
// Are we ereasing something important?
static void 
__clean_memory( 
    unsigned long start_address, 
    unsigned long end_address )
{
// Actually we're cleaning only 32 MB starting at the 1MB mark;

//++
// =======================================
// Cleaning the RAM.
// Vamos limpar somente a parte que será usada pelos 
// primeiros componentes do sistema.
// Mas mesmo assim precisamos limpar o bss das imagens
// e limpar as áreas alocadas.
// 32 mb, começando de 1mb.
// Is it slow?

    int max_in_mb = 32;

    unsigned long *CLEARBASE = (unsigned char *) start_address;
    register int i=0;

 // (32mb/4)
    int max = 
        ( (1024 * 1024 * max_in_mb ) / 4 );

// Limpamos somente se está dentro da área
// previamente descoberta.
// 4 bytes per time.
    for ( i=0; i<max; i++ )
    {
        if ( &CLEARBASE[i] < end_address )
        {
            CLEARBASE[i] = (unsigned long) 0;
        }
    };
// =======================================
//--
}

/*
 * blLoadKernelImage: 
 *     It loads the kernel image at 0x00100000.
 *     The entry point is at 0x00101000.
 */ 
// #todo
// This way can chose the filename from a
// configuration file.
// This routine will try to load the default filename
// if the provide name fail.
// This routine will build the pathname
// to search in the default folder.
static int blLoadKernelImage(void)
{
// Called by bl_main().

    int Status = -1;
// Standard name.
// #todo: Maybe we need some options, some config file.
    //char *image_name = "KERNEL.BIN";

// #bugbug
// Precisamos que essa rotina retorne
// para termos a change de inicializarmos o
// rescue shell. Mas acontece que por enquanto
// essa função aborta ao primeiro sinal de perigo.
// See: loader.c

    Status = (int) elfLoadKernelImage(image_name);
    if (Status < 0){
        printf ("blLoadKernelImage: elfLoadKernelImage fail\n");
        goto fail;
    }

    // OK
    return (int) Status;

fail:
    refresh_screen();
    return (int) (-1);
}

// Show menu.
static void blShowMenu(void)
{
    register int i=0;

// Cursor
    g_cursor_x = 0;
    g_cursor_y = 0;

    clear_backbuffer();

    for (i=0; i<8; i++)
    {
        printf ("\n");

        if (MENU[i].used == TRUE)
        {
            if ( i == menu_highlight ){
                printf("* %s \n", MENU[i].string);
            }else{
                printf("  %s \n", MENU[i].string);
            };
        }
    };

    refresh_screen(); 
}


static void blMenu(void)
{
    char Key=0;

    // prepara o menu.
    
    MENU[0].used = 1;
    MENU[1].used = 1;
    MENU[2].used = 0;
    MENU[3].used = 0;
    MENU[4].used = 0;
    MENU[5].used = 0;
    MENU[6].used = 0;
    MENU[7].used = 0;

    menu_highlight = 1;
        
    sprintf( MENU[0].string, "menu item 0");
    sprintf( MENU[1].string, "menu item 1");    

    //mostra o menu.
    //blShowMenu(); 

    while (1){
        blShowMenu();
        Key = keyboard_wait_key();
        switch (Key)
        {
            case 'x': 
                goto ____go; 
                break;

            case '\n':
                //goto do_execute_item;
                break;    
                
            // UP
            case 'e':
                //blMenuUP();
                break;
            
            // DOWN
            case 'd':
                //blMenuDOWN();
                break;    

            default:
                // Nothing
                break;
        };
    };

____go:
    return;
}


// blAbort:
// Rotina para abortar o bootloader em caso de erro grave.
void blAbort()
{
//@todo: 
//Talvez poderia ter uma interface antes de chamar a rotina abort().
//ex:
    //checks()
    abort(); 
}

// die:
// CLI HLT routine.
// No return!
// See: bootloader.h
void die(void)
{
    printf("BL.BIN: [DIE] System Halted\n");
    refresh_screen();
    while (1){
        asm ("cli");
        asm ("hlt");
    };
}


static void __no_x86_64_was_found(void)
{
// We probe the info and detected that the x86)64 instructions
// are not supported.
// We can hang, or load some 32bit funny stuff.

    printf("__no_x86_64_was_found: Sorry!\n");
    printf("It seems that the processor\n");
    printf("does not support x86_64 instructions.\n");

// #todo
// Maybe, in this case, we can load a 32bit version of Gramado,
// or another funny stuff.

    printf("The boot loader will not execute the kernel.\n");

done:
    refresh_screen();
    die();
    while (1){
        asm ("cli");
        asm ("hlt");
    };
}

// --------------------------------
// bl_main:
// Called by StartLoader in head.s.
// This is the entrypoint for the C part of the boot loader.
// + Load the kernel image.
// + Setup paging and the base pagetables used by the kernel.
// + Jumps to the kernel image.
void bl_main(void)
{
    int Status = (-1);
    int fTest=FALSE;

// root and fat not loaded yet.
    g_fat16_root_status = FALSE;
    g_fat16_fat_status = FALSE;
// main flags.
    gdefLegacyBIOSBoot  = FALSE;
    gdefEFIBoot         = FALSE;
    gdefSafeBoot        = FALSE;
    gdefShowLogo        = FALSE;
    gdefShowProgressBar = FALSE;
    // ...
// Set GUI.
    // VideoBlock.useGui = 1;
    // VideoBlock.useGui = 0;

// Display device
// Saving some display device info.

    DisplayDevice.width  = (unsigned long) SavedX;
    DisplayDevice.height = (unsigned long) SavedY;
    DisplayDevice.bpp    = (unsigned long) SavedBPP;

// Line in bytes.
    DisplayDevice.pitch = 
        (unsigned long) (SavedBPP * SavedX);
// Screen in bytes.
    DisplayDevice.screen_size_in_bytes = 
        (unsigned long) (SavedY * DisplayDevice.pitch);

    DisplayDevice.initialized = TRUE;

// -------------------------
// Initialize
// In this routine we're gonna fill the bootblock
// with all the data we have to the kernel.
// See: init.c
    Status = (int) init();
    if (Status != 0){
        // #todo
    }


// -- #bugbug -----------------------------------------------------

// Setup physical memory.
// Get last valid address.
    unsigned long first_address = 0x100000;  // 1MB mark
    unsigned long last_address=0;
    last_address = (unsigned long) __setup_physical_memory();

// Clean up memory.
// IN: start address, end address.
// #bugbug
// Are we ereasing something important?
// Actually we're cleaning only 32 MB starting at the 1MB mark;

    __clean_memory(
        (unsigned long) first_address,
        (unsigned long) last_address );
// -------------------------------------------------------

// Initialize bl heap.
// malloc support.
    init_heap();

// Initialize IDE support.
    init_hdd();

// debug
    if (g_initialized != TRUE){
        printf("bl_main: g_initialized\n");
        die();
    }

// Loading root dir.
    fs_load_rootdirEx();
    g_fat16_root_status = TRUE;

// Loading FAT.
    fs_load_fatEx();
    g_fat16_fat_status = TRUE;

// Loading kernel image.

    Status = blLoadKernelImage();
    if (Status<0){
         printf("bl_main: blLoadKernelImage fail\n");
         refresh_screen();
         while (1){
             asm("cli");
             asm("hlt");
         };
         //goto run_rescue_shell;
    }

// ok, depois de carregarmos a imagem do kernel de 64bit
// então devemos configurar o long mode e a paginação para
// 64bit ... para por fim saltarmos para o kernel.

/*
  #todo:
 bl.bin é feito em 32bit e ainda em 32bit ele deve carregar
 o kernel que é um programa de 64bit. 
  Ao fim de sua rotina em C 32bit ele faz as configurações
  necessarias para entrar em 64bit. Poderá usar rotinas
 em assembly 64bit. 
 E por fim salta para o kernel de 64bit.
*/

// Nesse momento ja carregamos a imagem do kernel.
// No momento é uma imagem fake.
// #todo
// precisamos limpara a tela,
// para visualizarmos essa mensagem na resolução 320x200

// #test
// background preto.

    g_cursor_x=0;
    g_cursor_y=0;
    clear_backbuffer();

/*
    printf ("\n");
    printf ("\n");
    printf ("Gramado BL.BIN: [main.c-OS_Loader_Main()] \n");
    printf ("The 64bit kernel image is \n");
    printf ("already loaded. So now we will \n");
    printf ("setup the long mode, the paging and\n");
    printf ("jump to the kernel image\n");
    printf ("Good luck\n");
    printf("======================\n");
*/

//#debug
/*
    printf ("OS_Loader_Main:\n");
    printf ("The kernel image is already loaded\n");
    printf ("Let's setup long mode,\n");
    printf ("paging and jump to the kernel.\n");
*/

    printf("bl_main: Kernel image loaded\n");

//#breakpoint
    //refresh_screen();
    //while(1){}

/*
    How do I enable Long Mode ?
    The steps for enabling long mode are:
    + Disable paging
    + Set the PAE enable bit in CR4
    + Load CR3 with the physical address of the PML4
    + Enable long mode by setting the EFER.LME flag in MSR 0xC0000080
    + Enable paging
    Now the CPU will be in compatibility mode, and 
instructions are still 32-bit. To enter long mode, 
+ the D/B bit (bit 22, 2nd 32-bit value) of the GDT code segment 
must be clear (as it would be for a 16-bit code segment), and 
the L bit (bit 21, 2nd 32-bit value) of the GDT code segment 
must be set. Once that is done, the CPU is in 64-bit long mode.
See: https://wiki.osdev.org/X86-64
*/


// ================================
// Check x86_64 support.
// Test LM-bit
// #todo: We can create a local herper for that thing.

    unsigned long a=0;
    unsigned long b=0;
    unsigned long c=0;
    unsigned long d=0;

    cpuid( 0x80000001, a, b, c, d );

    unsigned long data = (unsigned long) (d >> 29);

/*
// x86_64 is supported.
    if ( (data & 1) != 0 ){
        printf("OS_Loader_Main: x86_64 hardware supported\n");
        refresh_screen();
    }
*/

//
// x86_64 is not supported.
//

    if ((data & 1) == 0)
    {
        // Final message and hang
        __no_x86_64_was_found();
        // Hang
        while (1){
            asm ("cli");
            asm ("hlt");
        };
    }

    //printf ("done\n");
    //printf("======================\n");

    // #breakpoint
    //refresh_screen();
    //while(1){}

// No interrupts

    asm ("cli");

// ====
// Paging:
// Depois carregar o kernel e os modulos 
// nos seus endereços fisicos, 
// configura a paginaçao e 
// volta para o assembly para 
// configurar os registradores e 
// passar o comando para o kernel.
// #obs:
// Essa configuraçao basica nao impede
// que o kernel faça uma reconfiguraçao completa.
// #slow
// #debug
// In this document.
// See: pages.c

    //#debug
    //printf ("OS_Loader_Main: Setup paging ...\n");
    //refresh_screen();
    //while(1){}

    SetUpPaging();

// Nao podemos chamar rotina alguma aqui, somente retornar.
// Pois os registradores estao bagunçados.

// Not reached
    while (1){
        asm("cli");
        asm("hlt");
    };
}

//
// End
//


