#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096

typedef struct SubChainNode {
    size_t size;
    int type;
    void* v_ptr;
    struct SubChainNode* next;
    struct SubChainNode* prev;
} SubChainNode;

typedef struct MainChainNode {
    size_t psize;
    size_t used;
    SubChainNode* sub_chain;
    struct MainChainNode* next;
    struct MainChainNode* prev;
} MainChainNode;

MainChainNode* main_chain_head = NULL;
void* v_ptr = (void*)1000;

MainChainNode* addToMainChain(size_t size) {
    size_t main_node_size = size;
    size_t allocation_size = (main_node_size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;

    MainChainNode* new_main_node = (MainChainNode*)mmap(NULL, allocation_size,
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_main_node == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    new_main_node->psize = allocation_size;
    new_main_node->used = 0;
    new_main_node->sub_chain = NULL;
    new_main_node->next = main_chain_head;
    new_main_node->prev = NULL;

    if (main_chain_head) {
        main_chain_head->prev = new_main_node;
    }

    main_chain_head = new_main_node;
    return new_main_node;
}

SubChainNode* addToSubChain(MainChainNode* main_node, size_t size, int type) {
    SubChainNode* new_sub_node = (SubChainNode*)mmap(NULL, size,
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_sub_node == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    new_sub_node->size = size;
    new_sub_node->type = type;
    new_sub_node->next = NULL;
    new_sub_node->prev = NULL;
    new_sub_node->v_ptr = v_ptr;

    v_ptr = v_ptr + size;

    if (main_node->sub_chain) {
        new_sub_node->next = main_node->sub_chain;
        main_node->sub_chain->prev = new_sub_node;
    }

    main_node->sub_chain = new_sub_node;
    return new_sub_node;
}

void mems_init() {
    MainChainNode* main_chain_head = NULL;
    v_ptr = (void*)1000;
}

void mems_finish() {
    MainChainNode* main_node = main_chain_head;
    MainChainNode* next_main_node;

    while (main_node) {
        next_main_node = main_node->next;
        munmap(main_node, main_node->psize);
        main_node = next_main_node;
    }

    main_chain_head = NULL;
}

void* mems_malloc(size_t size) {
    MainChainNode* main_node = main_chain_head;

    if (main_chain_head == NULL || main_chain_head->used + size > main_chain_head->psize) {
        main_node = addToMainChain(size);
    }

    while (main_node) {
        if ((main_node->psize - main_node->used) >= size) {
            return addToSubChain(main_node, size, 1)->v_ptr;
        }

        if (main_node->next == NULL || main_node->next->used + size > main_node->next->psize) {
            main_node->next = addToMainChain(size);
        }

        main_node = main_node->next;
    }

    return NULL;
}

void printSubChain(SubChainNode* sub_chain) {
      unsigned long start_virtual_address = (unsigned long)sub_chain->v_ptr;
      unsigned long end_virtual_address = start_virtual_address + sub_chain->size - 1;

      printf("<P>[SVA:%lu:EVA:%lu] <-> ", start_virtual_address, end_virtual_address);
  }

  void mems_print_stats() {
     
      MainChainNode* main_node = main_chain_head;
      int total_pages_used = 0;
      size_t total_unused_memory = 0;
      int main_chain_length = 0;
      int sub_chain_lengths[100]; // Assuming a maximum of 100 sub-chains

      printf("MeMS Stats:\n");

      while (main_node != NULL) {
          printf("MAIN[%lu:%lu] -> ", (unsigned long)((char*)main_node),
                (unsigned long)((char*)main_node + main_node->psize - 1));

          SubChainNode* sub_node = main_node->sub_chain;
          int sub_chain_length = 0;

          while (sub_node != NULL) {
              printSubChain(sub_node);
              if (sub_node->type == 0) {
                  total_unused_memory += sub_node->size;
              }
              sub_node = sub_node->next;
              sub_chain_length++;
          }

          sub_chain_lengths[main_chain_length] = sub_chain_length;
          printf("<NULL>\n");
          main_chain_length++;
          total_pages_used += main_node->psize / PAGE_SIZE;
          main_node = main_node->next;
      }

      printf("Page used: %d\n", total_pages_used);
      printf("Space unused: %zu\n", total_unused_memory);
      printf("Main Chain Length: %d\n", main_chain_length);
      printf("Sub-chain Length array: ");

      for (int i = 0; i < main_chain_length; i++) {
          printf("%d ", sub_chain_lengths[i]);
      }
      printf("\n");
  }

void* mems_get(void* v_ptr) {
    MainChainNode* main_node = main_chain_head;

    while (main_node) {
        SubChainNode* sub_node = main_node->sub_chain;

        while (sub_node) {
            if (sub_node->v_ptr == v_ptr) {
                size_t offset = (size_t)(v_ptr - sub_node->v_ptr);
                return (void*)((char*)main_node + offset);
            }

            sub_node = sub_node->next;
        }

        main_node = main_node->next;
    }

    return NULL;
}

void combineFreeSubNodes(MainChainNode* main_node) {
    SubChainNode* current_sub_node = main_node->sub_chain;

    while (current_sub_node && current_sub_node->next) {
        if (current_sub_node->type == 0 && current_sub_node->next->type == 0) {
            current_sub_node->size += current_sub_node->next->size;
            current_sub_node->next = current_sub_node->next->next;

            if (current_sub_node->next) {
                current_sub_node->next->prev = current_sub_node;
            }
        } else {
            current_sub_node = current_sub_node->next;
        }
    }
}

void mems_free(void* v_ptr) {
    MainChainNode* main_node = main_chain_head;

    while (main_node) {
        SubChainNode* sub_node = main_node->sub_chain;

        while (sub_node) {
            if (sub_node->v_ptr == v_ptr) {
                sub_node->type = 0;
                main_node->used -= sub_node->size;
                combineFreeSubNodes(main_node);
                return;
            }

            sub_node = sub_node->next;
        }

        main_node = main_node->next;
    }
}
