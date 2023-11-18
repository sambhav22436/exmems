
#include<stdio.h>
#include<stdlib.h>
#include <sys/mman.h>
#define PAGE_SIZE 4096
typedef struct SubChainNode {           
    size_t size;
    void* data;            
    int type;
    void* v_ptr;               
    struct SubChainNode* next;
    struct SubChainNode* prev;
} SubChainNode;

// Define a node for the main chain
typedef struct MainChainNode {
    size_t psize;
    size_t used;
    size_t wasted;

    SubChainNode* sub_chain; // Pointer to the sub-chain
    struct MainChainNode* next;
    struct MainChainNode* prev;

} MainChainNode;

MainChainNode* main_chain_head = NULL;
void* v_ptr = (void*)1000;



// Function to add a new node to the main chain
MainChainNode* addToMainChain(size_t size) {
    // Calculate the size to allocate based on the page size
    size_t main_node_size = size;
    size_t allocation_size = (main_node_size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
    

    MainChainNode* new_main_node = (MainChainNode*)mmap(NULL, allocation_size,
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_main_node == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    new_main_node->psize=allocation_size;
    new_main_node->used=0;

    new_main_node->sub_chain = NULL;
    new_main_node->next = main_chain_head;
    new_main_node->prev = NULL;
    

    if (main_chain_head) {
        main_chain_head->prev = new_main_node;
    }

    main_chain_head = new_main_node;
    new_main_node->sub_chain = (SubChainNode*)(new_main_node + 1);
    return new_main_node;

}

SubChainNode* addToSubChain(MainChainNode* main_node, size_t size, int type) {
    SubChainNode* new_sub_node = (SubChainNode*)(main_node->sub_chain);
    
    new_sub_node->size = size;
    new_sub_node->type = type;
    new_sub_node->next = NULL;
    new_sub_node->prev = NULL;

    if (main_node->sub_chain) {
        new_sub_node->next = (SubChainNode*)((char*)main_node->sub_chain + size);
        new_sub_node->next->prev = new_sub_node;
    }
    
    new_sub_node->v_ptr = v_ptr; // Set the current virtual address
    v_ptr = v_ptr + size;
    main_node->sub_chain = new_sub_node->next;
    return new_sub_node;
}


void mems_init(){
    MainChainNode* main_chain_head = NULL;
    void* v_ptr = (void*)1000;

}


/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish(){
       MainChainNode* main_node = main_chain_head;
    MainChainNode* next_main_node;

    while (main_node) {
      

        next_main_node = main_node->next;
        munmap(main_node, main_node->psize);
        main_node = next_main_node;
    }

    main_chain_head = NULL; 
    
}


/*
Allocates memory of the specified size by reusing a segment from the free list if 
a sufficiently large segment is available. 

Else, uses the mmap system call to allocate more memory on the heap and updates 
the free list accordingly.

Note that while mapping using mmap do not forget to reuse the unused space from mapping
by adding it to the free list.
Parameter: The size of the memory the user program wants
Returns: MeMS Virtual address (that is created by MeMS)
*/ 
void* mems_malloc(size_t size){
    MainChainNode* main_node = main_chain_head;

    if (main_chain_head == NULL) {
        main_chain_head = addToMainChain(size); // Assuming addToMainChain returns the new main_node
        main_node = main_chain_head;
    }

    while (main_node) {
        if ((main_node->psize - main_node->used) >= size) {
            SubChainNode* sub_node = main_node->sub_chain;

            while (sub_node) {
                if (sub_node->type == 0 && sub_node->size > size) {
                    sub_node->type = 1;
                    main_node->used+=sub_node->size;

                    return sub_node->v_ptr;
                }
                if(sub_node->next==NULL){
                 sub_node->next=addToSubChain(main_node,size,1);
                 main_node->used+=size;
                 return sub_node->next->v_ptr;
                 
                 }
                sub_node = sub_node->next;
            
            
            }

            // If you reach this point, no suitable sub_node was found in the current main_node
        }

        if (main_node->next == NULL) {
            main_node->next = addToMainChain(size); // Add a new main_node if necessary
        }

        main_node = main_node->next;
    }

    // Handle the case where no suitable sub_node or main_node is found
    return NULL;

}


/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/
void mems_print_stats(){
    MainChainNode* main_node = main_chain_head;
    int totalPagesUsed = 0;
    int totalUnusedMemory = 0;
    int totalWastedMemory = 0;
    int main_count =0;

    while (main_node) {
        totalPagesUsed+=main_node->psize/PAGE_SIZE;
        totalUnusedMemory += main_node->psize - main_node->used;
        main_count++;

        printf("Main Chain Node: psize=%zu, used=%zu\n",
            main_node->psize, main_node->used);

        SubChainNode* sub_node = main_node->sub_chain;
        int segmentCount = 0;

        while (sub_node) {
            printf("  Segment %d: size=%zu, type=%d, v_ptr=%p\n",
                segmentCount, sub_node->size, sub_node->type, sub_node->v_ptr);
            sub_node = sub_node->next;
            segmentCount++;
        }
        printf("length of subchain %d\n",main_count, segmentCount);

        main_node = main_node->next;
    }

    printf("Total pages utilized: %d\n", totalPagesUsed);
    printf("Total unused memory: %d\n", totalUnusedMemory);
    printf("length of mainchain %d\n",main_count);

}


/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void* mems_get(void* v_ptr) {
    MainChainNode* main_node = main_chain_head;

    while (main_node) {
        SubChainNode* sub_node = main_node->sub_chain;

        while (sub_node) {
            if (sub_node->v_ptr == v_ptr) {
                // Calculate the physical address based on the MeMS layout
                size_t offset = (size_t)(v_ptr - main_node->sub_chain->v_ptr);
                return (void*)((char*)main_node + offset);
            }

            sub_node = sub_node->next;
        }

        main_node = main_node->next;
    }

    return NULL; // If v_ptr is not found in the MeMS system
}



/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS) 
Returns: nothing
*/
void mems_free(void *v_ptr){
    MainChainNode* main_node = main_chain_head;

    while (main_node) {
        SubChainNode* sub_node = main_node->sub_chain;

        while (sub_node) {
            if (sub_node->v_ptr == v_ptr) {
                // Mark the segment as free (type 0) and reset the wasted value
                sub_node->type = 0;
                main_node->used-=sub_node->size;

                return; // Memory freed and added to the free list
            }

            sub_node = sub_node->next;
        }
        combineFreeSubNodes(main_node);

        main_node = main_node->next;
    }
    
}
void combineFreeSubNodes(MainChainNode* main_node) {
    SubChainNode* current_sub_node = main_node->sub_chain;
    SubChainNode* next_sub_node;

    while (current_sub_node && current_sub_node->next) {
        if (current_sub_node->type == 0 && current_sub_node->next->type == 0) {
            // Combine adjacent free sub-nodes
            current_sub_node->size += current_sub_node->next->size;

            // Update the v_ptr of the combined sub-node
            current_sub_node->next->v_ptr = current_sub_node->v_ptr + current_sub_node->size;

            // Calculate the physical address of the combined sub-node
            void* physical_address = mems_get_physical_address(current_sub_node->v_ptr);
            current_sub_node->next = (SubChainNode*)((char*)physical_address + current_sub_node->size);

            // Update the links
            if (current_sub_node->next) {
                current_sub_node->next->prev = current_sub_node;
            }
            current_sub_node->next = current_sub_node->next->next;
        } else {
            current_sub_node = current_sub_node->next;
        }
    }
}
