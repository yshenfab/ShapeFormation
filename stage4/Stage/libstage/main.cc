/**
  \defgroup stage The Stage standalone robot simulator.

  USAGE:  stage [options] <worldfile1> [worldfile2 ... ]

  Available [options] are:

    --clock        : print simulation time peridically on standard output

    -c             : equivalent to --clock

    --gui          : run without a GUI

    -g             : equivalent to --gui

    --help         : print this message

    --args \"str\"   : define an argument string to be passed to all controllers

    -a \"str\"       : equivalent to --args "str"

    -h             : equivalent to --help"

    -?             : equivalent to --help
 */

// #include<random>
// #include<ctime>

#include <getopt.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

#include "config.h"
#include "stage.hh"
using namespace Stg;

const char *USAGE = "USAGE:  stage [options] <num_robots> <image_or_map> <worldfile>\n"
                    "Available [options] are:\n"
                    "  --clock        : print simulation time peridically on standard output\n"
                    "  -c             : equivalent to --clock\n"
                    "  --gui          : run without a GUI\n"
                    "  -g             : equivalent to --gui\n"
                    "  --help         : print this message\n"
                    "  --args \"str\"   : define an argument string to be passed to all "
                    "controllers\n"
                    "  -a \"str\"       : equivalent to --args \"str\"\n"
                    "  -h             : equivalent to --help\n"
                    "  -?             : equivalent to --help\n"
                    "Example: stage -g 100 formation/img/apple.png formation100.world";

/* options descriptor */

static struct option longopts[] = { { "gui", optional_argument, NULL, 'g' },
                                    { "clock", optional_argument, NULL, 'c' },
                                    { "help", optional_argument, NULL, 'h' },
                                    { "args", required_argument, NULL, 'a' },
                                    { NULL, 0, NULL, 0 } };

int main(int argc, char *argv[])
{
  // initialize libstage - call this first
  Stg::Init(&argc, &argv);

  int ch = 0, optindex = 0;
  bool usegui = true;
  /* #ifdef __APPLE__ */
  /*   usegui = true; */
  /* #else */
  /*   usegui = false; */
  /* #endif */
  bool showclock = false;

  while ((ch = getopt_long(argc, argv, "cgh?", longopts, &optindex)) != -1) {
    switch (ch) {
    case 0: // long option given
      printf("option %s given\n", longopts[optindex].name);
      break;
    case 'a': World::ctrlargs = std::string(optarg); break;
    case 'c':
      showclock = true;
      printf("[Clock enabled]");
      break;
    case 'g':
      usegui = false;
      printf("[GUI disabled]");
      break;
    case 'h': puts(USAGE); return EXIT_SUCCESS;
    case '?': puts(USAGE); return EXIT_FAILURE;
    default:
      printf("unhandled option %c\n", ch);
      puts(USAGE);
      // exit(0);
    }
  }

  puts(""); // end the first start-up line

  const int positional_count = argc - optind;
  if (positional_count != 3) {
    std::cerr << "Expected exactly num_robots, image_or_map, and one world file.\n";
    puts(USAGE);
    return EXIT_FAILURE;
  }

  const char *robot_count_text = argv[optind];
  char *parse_end = NULL;
  errno = 0;
  const long robot_count = strtol(robot_count_text, &parse_end, 10);
  if (errno != 0 || parse_end == robot_count_text || *parse_end != '\0' || robot_count <= 0) {
    std::cerr << "num_robots must be a positive integer: '" << robot_count_text << "'\n";
    return EXIT_FAILURE;
  }

  const char *input_path = argv[optind + 1];
  const char *worldfilename = argv[optind + 2];

  // Stg::Init() captures the raw argv before getopt processes it. Controllers
  // need a stable, option-free contract regardless of whether -g/-c was used.
  World::args.clear();
  World::args.push_back(argv[0]);
  World::args.push_back(robot_count_text);
  World::args.push_back(input_path);
  World::args.push_back(worldfilename);

  World *world = (usegui ? new WorldGui(400, 300, worldfilename) : new World(worldfilename));
  if (!world->Load(worldfilename)) {
    std::cerr << "Unable to load world file: '" << worldfilename << "'\n";
    delete world;
    return EXIT_FAILURE;
  }
  world->ShowClock(showclock);

  if (!world->paused)
    world->Start();

  World::Run();

  puts("\n[Done!]");

  return EXIT_SUCCESS;
}
