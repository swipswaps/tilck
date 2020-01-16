/* SPDX-License-Identifier: BSD-2-Clause */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>

#include "devshell.h"
#include "sysenter.h"

struct test_cmd_entry *cmds_table;

static const char *tt_str[] =
{
   [TT_SHORT] = "tt_short",
   [TT_MED] = "tt_med",
   [TT_LONG] = "tt_long",
};

static int get_cmds_count(void) {

   static int value;
   struct test_cmd_entry *e;

   if (value)
      return value;

   for (e = cmds_table; e->name != NULL; e++) {
      value++;
   }

   return value;
}

int cmd_selftest(int argc, char **argv)
{
   char buf[256];

   if (argc < 1) {
      fprintf(stderr, PFX "Expected selftest name argument.\n");
      return 1;
   }

   int rc = sysenter_call3(TILCK_CMD_SYSCALL,
                           TILCK_CMD_RUN_SELFTEST,
                           argv[0], /* self test name */
                           NULL);

   if (rc != 0) {

      sprintf(buf, "%s_manual", argv[0]);
      rc = sysenter_call3(TILCK_CMD_SYSCALL,
                          TILCK_CMD_RUN_SELFTEST,
                          buf,  /* self test name */
                          NULL);

      if (rc != 0) {
         fprintf(stderr, PFX "Invalid selftest '%s'\n", argv[0]);
         return 1;
      }
   }

   return 0;
}

static u64 get_monotonic_time_ms(void)
{
   int rc;
   struct timespec tp;

   rc = clock_gettime(CLOCK_MONOTONIC, &tp);

   if (rc < 0) {
      perror("clock_gettime(CLOCK_MONOTONIC) failed");
      exit(1);
   }

   return (u64)tp.tv_sec * 1000 + (u64)tp.tv_nsec / 1000000;
}

static bool
run_child(int argc, char **argv, cmd_func_type func, const char *name)
{
   static const char *pass_fail_strings[2] = {
      COLOR_RED STR_FAIL RESET_ATTRS,
      COLOR_GREEN STR_PASS RESET_ATTRS,
   };

   int child_pid, wstatus, rc;
   u64 start_ms, end_ms;
   bool pass;

   start_ms = get_monotonic_time_ms();

   printf(COLOR_YELLOW PFX);
   printf(COLOR_GREEN STR_RUN RESET_ATTRS "%s"  "\n", name);

   child_pid = fork();

   if (child_pid < -1) {
      perror("fork failed");
      exit(1);
   }

   if (!child_pid) {
      exit(func(argc, argv));
   }

   rc = waitpid(child_pid, &wstatus, 0);

   if (rc < 0) {
      perror("waitpid() failed");
      return false;
   }

   if (rc != child_pid) {
      fprintf(stderr, "waitpid() returned value [%d] "
                      "!= child_pid [%d]\n", rc, child_pid);
      return false;
   }

   end_ms = get_monotonic_time_ms();

   pass = WIFEXITED(wstatus) && (WEXITSTATUS(wstatus) == 0);
   printf(COLOR_YELLOW PFX "%s", pass_fail_strings[pass]);
   printf("%s (%llu ms)\n\n", name, end_ms - start_ms);
   return pass;
}

int cmd_runall(int argc, char **argv)
{
   bool any_failure = false;
   int wstatus;
   int child_pid;
   int to_run = 0, passed = 0;
   u64 start_ms, end_ms;

   start_ms = get_monotonic_time_ms();

   for (int i = 1; i < get_cmds_count(); i++) {

      if (!cmds_table[i].enabled_in_st || cmds_table[i].tt == TT_LONG)
         continue;

      to_run++;

      if (!run_child(argc, argv, cmds_table[i].func, cmds_table[i].name)) {
         any_failure = true;
         continue;
      }

      passed++;
   }

   end_ms = get_monotonic_time_ms();

   printf(COLOR_YELLOW PFX);
   printf("------------------------------------------------------------\n");
   printf(COLOR_YELLOW PFX);

   printf(passed == to_run ? COLOR_GREEN : COLOR_RED);
   printf("Tests passed %d/%d" RESET_ATTRS " ", passed, to_run);
   printf("(%llu ms)\n\n", end_ms - start_ms);

   if (dump_coverage) {
      dump_coverage_files();
   }

   exit(any_failure);
}

void dump_list_of_commands_and_exit(void)
{
   for (int i = 1; i < get_cmds_count(); i++) {
      if (cmds_table[i].enabled_in_st)
         printf("%s %s\n", cmds_table[i].name, tt_str[cmds_table[i].tt]);
   }

   exit(0);
}

void show_common_help_intro(void)
{
   printf("\n");
   printf(COLOR_RED "Tilck development shell\n" RESET_ATTRS);

   printf("This application is a small dev-only utility originally written ");
   printf("in order to \nallow running simple programs, while proper shells ");
   printf("like ASH couldn't run on \nTilck yet. Today, " COLOR_YELLOW);
   printf("ASH works on Tilck" RESET_ATTRS " and this ");
   printf(COLOR_YELLOW "devshell" RESET_ATTRS " is used as a runner \n");
   printf("for system tests (often called 'shellcmds' for this reason).\n\n");
}

int cmd_help(int argc, char **argv)
{
   size_t n, row_len;
   char buf[64];

   show_common_help_intro();

   printf(COLOR_RED "Built-in commands\n" RESET_ATTRS);
   printf("    help         shows this help\n");
   printf("    cd <DIR>     change the current working directory\n");
   printf("    runall       run all the shellcmd tests\n\n");
   printf(COLOR_RED "Kernel test commands\n" RESET_ATTRS);

   row_len = printf("    ");

   for (int i = 1 /* skip help */; i < get_cmds_count(); i++) {

      n = sprintf(buf, "%s%s", cmds_table[i].name,
                  i != get_cmds_count()-1 ? "," : "");

      if (row_len + n >= 80) {
         printf("\n");
         row_len = printf("    ");
      } else if (i > 1) {
         row_len += printf(" ");
      }

      row_len += printf("%s", buf);
   }

   printf("\n\n");
   return 0;
}

static void run_cmd(cmd_func_type func, int argc, char **argv)
{
   int exit_code = func(argc, argv);

   if (dump_coverage) {
      dump_coverage_files();
   }

   exit(exit_code);
}

void run_if_known_command(const char *cmd, int argc, char **argv)
{
   for (int i = 0; i < get_cmds_count(); i++)
      if (!strcmp(cmds_table[i].name, cmd))
         run_cmd(cmds_table[i].func, argc, argv);
}
