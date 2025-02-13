## Project：doorbell_ab_4M

## Life Cycle：2023-06-16 ~~ 2023-12-06

## Application：media doorbell_ab_4M

## Special Macro Configuration Description：
CONFIG_MEDIA=y                // support media project
CONFIG_WIFI_TRANSFER=y        // support wifi transfer encode frame
CONFIG_IMAGE_STORAGE=y        // support capture frame and save to sdcard
CONFIG_INTEGRATION_DOORBELL=y // support ArminoMedia apk

## Complie Command:	
1、make bk7258 PROJECT=media/doorbell_ab_4M

## CPU: riscv

## RAM:
mem_type start      end        size    
-------- ---------- ---------- --------
itcm     0x10000000 0x100007c4 1988    
dtcm     0x20000400 0x20001de8 6632    
ram      0x3000c800 0x3001b6c0 61120   
data     0x3000c800 0x3000e0a0 6304    
bss      0x3000e0a0 0x3001b6c0 54816   
heap     0x38000000 0x38040000 262144  
psram    0x60700000 0x60800000 1048576 

## Media: video
1、psram range used for media: 0x60000000 - 0x606FFFFF
psram_based_addr: 0x60000000
according encode frame resolution to allocate psram block, please reference components/media/frame_buffer.c

2、psram work in 8M config, if need work 16M config, should adjust marco:
CONFIG_PSRAM_MEM_SLAB_USER_SIZE
CONFIG_PSRAM_MEM_SLAB_AUDIO_SIZE
CONFIG_PSRAM_MEM_SLAB_ENCODE_SIZE
CONFIG_PSRAM_MEM_SLAB_DISPLAY_SIZE

CONFIG_PSRAM_AS_SYS_MEMORY
CONFIG_PSRAM_HEAP_CPU0_BASE_ADDER
CONFIG_PSRAM_HEAP_BASE
CONFIG_PSRAM_HEAP_SIZE

