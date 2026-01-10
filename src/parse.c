#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "parse.h"

int update_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *updatestring) {
    if (NULL == dbhdr) return STATUS_ERROR;
    if (NULL == employees) return STATUS_ERROR;
    if (NULL == *employees) return STATUS_ERROR;
    if (NULL == updatestring) return STATUS_ERROR;

    char *name = strtok(updatestring, ",");
    if (NULL == name) return STATUS_ERROR;
    char *address = strtok(NULL, ",");
    if (NULL == address) return STATUS_ERROR;
    char *hours = strtok(NULL, ",");
    if (NULL == hours) return STATUS_ERROR;

    int i;
    struct employee_t *e = *employees;

    for (i = 0; i < dbhdr->count; i++) {
        if (strcmp(e[i].name, name) == 0) {
            strncpy(e[i].address, address, sizeof(e[i].address)-1);
            e[i].hours = atoi(hours);
        }
    }
    
    *employees = e;

    return STATUS_SUCCESS;
}

int remove_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *removename) {
    if (NULL == dbhdr) return STATUS_ERROR;
    if (NULL == employees) return STATUS_ERROR;
    if (NULL == *employees) return STATUS_ERROR;
    if (NULL == removename) return STATUS_ERROR;

    int i, employees_remove = 0;
    for (i = 0; i < dbhdr->count; i++) {
        if (strcmp((*employees)[i].name, removename) == 0) {
            employees_remove++;
        }
    }
    
    int new_e_count = dbhdr->count - employees_remove;
    struct employee_t *new_e = (struct employee_t *) calloc(new_e_count, sizeof(struct employee_t));
    if (new_e == NULL) {
        printf("Failed to malloc\n");
        return STATUS_ERROR;
    }

    int j = 0;
    for (i = 0; i < dbhdr->count; i++) {
        if (strcmp((*employees)[i].name, removename) != 0) {
            new_e[j] = (*employees)[i];
            j++;
        }
    }

    dbhdr->count = new_e_count;
    free(*employees);
    *employees = new_e;

    return STATUS_SUCCESS;
}

void list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {
    if (NULL == dbhdr) return;
    if (NULL == employees) return;
    int i;
    for (i = 0; i < dbhdr->count; i++) {
        printf("Employee %d\n", i);
        printf("\tName: %s\n", employees[i].name);
        printf("\tAddress: %s\n", employees[i].address);
        printf("\tHours: %d\n", employees[i].hours);
    }
    return;
}

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring) {
    if (NULL == dbhdr) return STATUS_ERROR;
    if (NULL == employees) return STATUS_ERROR;
    if (NULL == addstring) return STATUS_ERROR;

    char *name = strtok(addstring, ",");
    if (NULL == name) return STATUS_ERROR;
    char *address = strtok(NULL, ",");
    if (NULL == address) return STATUS_ERROR;
    char *hours = strtok(NULL, ",");
    if (NULL == hours) return STATUS_ERROR;

    struct employee_t *e = *employees;
    e = (struct employee_t *) realloc(e, (dbhdr->count + 1)*sizeof(struct employee_t));
    if (e == NULL) {
        return STATUS_ERROR;
    }

    dbhdr->count++;

    printf("Name: %s| Addres: %s| Hours: %s\n", name, address, hours);
    strncpy(e[dbhdr->count-1].name, name, sizeof(e[dbhdr->count-1].name)-1);
    strncpy(e[dbhdr->count-1].address, address, sizeof(e[dbhdr->count-1].address)-1);
    e[dbhdr->count-1].hours = atoi(hours);
    *employees = e;
    return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut) {
    if (fd < 0) {
        printf("Got bad fd from the user\n");
        return STATUS_ERROR;
    }

    int count = dbhdr->count;
    struct employee_t *employees = (struct employee_t *) calloc(count, sizeof(struct employee_t));
    if (employees == NULL) {
        printf("Malloc failed\n");
        return STATUS_ERROR;
    }

    read(fd, employees, count*sizeof(struct employee_t));

    int i;
    for (i = 0; i < count; i++) {
        employees[i].hours = ntohl(employees[i].hours);
    }

    *employeesOut = employees;

    return STATUS_SUCCESS;
}


int output_file(int fd, struct dbheader_t *dbhdr, struct employee_t *employees) {
    if (fd < 0) {
        printf("Got bad fd from the user\n");
        return STATUS_ERROR;
    }

    int realcount = dbhdr->count;

    dbhdr->version = htons(dbhdr->version);
    dbhdr->count = htons(dbhdr->count);
    dbhdr->magic = htonl(dbhdr->magic);
    dbhdr->filesize = htonl(sizeof(struct dbheader_t) + sizeof(struct employee_t)*realcount);

    lseek(fd, 0, SEEK_SET);

    write(fd, dbhdr, sizeof(struct dbheader_t));
    int i;
    for (i = 0; i < realcount; i++) {
        employees[i].hours = htonl(employees[i].hours);
        write(fd, &employees[i], sizeof(struct employee_t));
    }

    off_t new_size = sizeof(struct dbheader_t) + sizeof(struct employee_t) * realcount;
    ftruncate(fd, new_size);

    return STATUS_SUCCESS;
}	

int validate_db_header(int fd, struct dbheader_t **headerOut) {
    if (fd < 0) {
        printf("Got bad fd from the user\n");
        return STATUS_ERROR;
    }

    struct dbheader_t *header = (struct dbheader_t *) calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        printf("Malloc failed to create dbheader_t\n");
        return STATUS_ERROR;
    }

    if (read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        perror("read");
        free(header);
        return STATUS_ERROR;
    }

    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->magic = ntohl(header->magic);
    header->filesize = ntohl(header->filesize);

    if (header->magic != HEADER_MAGIC) {
        printf("Improper header magic\n");
        free(header);
        return -1;
    }

    if (header->version != 1) {
        printf("Improper header version\n");
        free(header);
        return -1;
    }

    if (header->version != 1) {
        printf("Improper header version\n");
        free(header);
        return -1;
    }

    struct stat dbstat = {0};
    fstat(fd, &dbstat);
    if (header->filesize != dbstat.st_size) {
        printf("Corrupte database\n");
        free(header);
        return -1;
    }

    *headerOut = header;
    return STATUS_SUCCESS;
}

int create_db_header(struct dbheader_t **headerOut) {
	struct dbheader_t *header = (struct dbheader_t *) calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        printf("Malloc failed to create dbheader_t\n");
        return STATUS_ERROR;
    }
     
    header->version = 0x1;
    header->count = 0;
    header->magic = HEADER_MAGIC;
    header->filesize = sizeof(struct dbheader_t);

    *headerOut = header;

    return STATUS_SUCCESS;
}


