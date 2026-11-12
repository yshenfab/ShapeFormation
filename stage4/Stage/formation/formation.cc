/*
 * Two versions to compare: (this is v2)
 * v1: swap paper, v2: ours
 * Robot roles (anchoring or common robot)
 *
 * v1: {up,down,left,right} as surroundings, use surroundings as candidate target, target
 * confliction based on lexical small, choose candidate target or random target in all targets
 * (similar to shape formation paper)
 *
 * v2: target confliction based on distance & lex, choose candidate target or random target
 * in known_unassigned_targets
 *
 * */

#include <string.h>
#include <time.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <opencv2/opencv.hpp>
#include "stage.hh"
#include "collision_avoidance.hh"
#include "connected_target_generation.hh"
#include "experiment_config.hh"
#include "experiment_metrics.hh"
#include "target_image_generation.hh"
#include "target_spatial_index.hh"

/* #define rep(i, a, b) for (int i = a; i < b; ++i) */
/* #define Rep(i, a, b) for (int i = a; i < b; --i) */
/* #define foreach(e, x) for (__typeof(x.begin()) e = x.begin(); e != x.end(); ++e) */
/* #define MEM(a, x) memset(a, x, sizeof a) */

using namespace Stg;
using namespace cv;
using namespace std;

typedef struct {
  ModelPosition *position;
  ModelFiducial *fiducial;

  ModelFiducial::Fiducial *detection;

  vector<int> known_assigned_target_idx;
  int nearest_unoccupied_target_idx;
  formation::PathLengthTracker path_length;
  // vector<int> known_unoccupied_target_idx;
} robot_t;

typedef enum { AVOID_ALL, AVOID_CLOSEST, AVOID_NONE } AvoidCollisionMode;
typedef enum { RAND_INIT, CLOEST_INIT, ID_INIT } InitMode;

// epuck config
// epuck radius: r = 0.0355m
// IR sensors range: R = 0.25m
// target dist TD >= 0.1004m
// grid edge length l: 0.1004 <= l <= 0.125

// swap paper config
// robot radius: r 0.05m
// robot speed 0.05m/s
// safe dist 2r 0.1m
// communication range R >= 4*sqrt(2)*r=5.66*r 0.6m
// target dist TD >= 2*sqrt(2)*r=2.83*r  0.14m
// grid edge length l: 2*sqrt(2)*r <= l <= R/2   0.14 <= l <=0.3

// simulation config
// robot radius: 1.0
// velocity: 1.0
// sensor range: 12.0
// safe dist: 2.5
// target dist TD: safe dist < TD < sensor range, 2.5 < TD < 12.0

int num_robots = 0;
double radius = 1.0;
const double sensor_range = 12.0 * radius;
formation::TargetSpatialIndex target_spatial_index(sensor_range);
double VSPEED = radius; // equals to robot radius
// const double MIN_TGT_DIST = 2.0 * sqrt(2) * radius; // use as safe dist
// const double MAX_TGT_DIST = sensor_range * 0.5;

// map boundrary, defined in file "formation.world" (floorplan size)
double xbound = 0.0;
double ybound = 0.0;
double SAFE_DIST = 2.5 * radius; // 2 * sqrt(2) * radius

vector<Pose> g_targets;
double grid_length = std::numeric_limits<double>::quiet_NaN();
double target_mst_bottleneck = std::numeric_limits<double>::quiet_NaN();
bool targets_initialized = false;
bool experiment_settings_initialized = false;
bool result_written = false;
bool world_callback_registered = false;
vector<robot_t *> g_robots;
std::mt19937 controller_rng;
unsigned long experiment_seed = 20270809UL;
double simulation_timeout_seconds = 7200.0;
double progress_interval_seconds = 10.0;
double next_progress_report_seconds = 10.0;
const double arrival_tolerance = 0.5;

// some options
bool show_target_info = false;
bool save_generated_img = true;
bool use_swap = true;
bool use_img = true; // img or txt
bool show_debug_info = false;

// forward declare
int FiducialUpdate(ModelFiducial *fid, robot_t *robot);

double compute_dist(Pose p1, Pose p2);

int nearest_unassigned_target(vector<int> assigned, Pose pose);
int random_unassigned_target(vector<int> assigned);
vector<int> get_unassigned_target_idx(vector<int> assigned);
vector<int> get_targets_in_range(Pose pose);
int nearest_unassigned_target_in_detection_range(vector<int> assigned, Pose pose);

bool target_swap_reduce_cost(Pose p1, Pose t1, Pose p2, Pose t2);
bool is_lexical_small(Pose p1, Pose p2);
Pose update_pose(Pose pose, Pose target_pose);
string get_img_name(string s, string delim, int x);

// generate targets' positions
double generate_targets(vector<Pose> &targets, string img_path);
double generate_targets_new(vector<Pose> &targets, string img_path);
double read_data(vector<Pose> &targets, string filename);
int a_in_b(Pose a, vector<Pose> b);
vector<int> get_targets_one_step_away(Pose pose);

bool initialize_experiment_settings(World *world);
int ExperimentUpdate(World *world, void *user);
void finish_run(const string &status, World *world, double max_target_error);
double random_unit();
int random_index(size_t count);

void pvec(vector<int> vec)
{
  for (int i = 0; i < vec.size(); ++i) {
    cout << vec[i] << ",";
  }
  cout << endl;
}

namespace {

string environment_value(const char *name, const string &fallback = string())
{
  const char *value = std::getenv(name);
  return (value != NULL && value[0] != '\0') ? string(value) : fallback;
}

bool parse_positive_double(const string &text, double *value)
{
  if (text.empty() || value == NULL)
    return false;
  char *end = NULL;
  errno = 0;
  const double parsed = std::strtod(text.c_str(), &end);
  if (errno != 0 || end == text.c_str() || *end != '\0' || !std::isfinite(parsed)
      || parsed <= 0.0) {
    return false;
  }
  *value = parsed;
  return true;
}

bool parse_unsigned_long(const string &text, unsigned long *value)
{
  if (text.empty() || value == NULL)
    return false;
  char *end = NULL;
  errno = 0;
  const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0')
    return false;
  *value = parsed;
  return true;
}

string json_escape(const string &value)
{
  ostringstream escaped;
  for (size_t index = 0; index < value.size(); ++index) {
    const unsigned char character = static_cast<unsigned char>(value[index]);
    switch (character) {
    case '\\': escaped << "\\\\"; break;
    case '"': escaped << "\\\""; break;
    case '\n': escaped << "\\n"; break;
    case '\r': escaped << "\\r"; break;
    case '\t': escaped << "\\t"; break;
    default:
      if (character < 0x20) {
        escaped << "\\u" << hex << setw(4) << setfill('0') << static_cast<int>(character) << dec
                << setfill(' ');
      } else {
        escaped << value[index];
      }
    }
  }
  return escaped.str();
}

void write_json_number(ostream &output, double value)
{
  if (std::isfinite(value))
    output << setprecision(15) << value;
  else
    output << "null";
}

formation::CompletionResult current_completion()
{
  vector<formation::RobotAssignment> robots;
  vector<formation::Point2D> targets;
  robots.reserve(g_robots.size());
  targets.reserve(g_targets.size());

  for (size_t index = 0; index < g_targets.size(); ++index)
    targets.push_back({ g_targets[index].x, g_targets[index].y });

  for (size_t index = 0; index < g_robots.size(); ++index) {
    const int target_index = g_robots[index]->position->getTargetIdx();
    const Pose pose = g_robots[index]->position->GetPose();
    const size_t checked_index =
        target_index < 0 ? std::numeric_limits<size_t>::max() : static_cast<size_t>(target_index);
    robots.push_back({ checked_index, { pose.x, pose.y } });
  }
  return formation::evaluate_completion(robots, targets, arrival_tolerance);
}

} // namespace

bool initialize_experiment_settings(World *world)
{
  if (experiment_settings_initialized)
    return num_robots > 0 && xbound > 0.0;
  experiment_settings_initialized = true;

  if (World::args.size() != 4) {
    cerr << "Formation controller requires normalized arguments: N input world\n";
    return false;
  }

  char *end = NULL;
  errno = 0;
  const long parsed_robot_count = std::strtol(World::args[1].c_str(), &end, 10);
  if (errno != 0 || end == World::args[1].c_str() || *end != '\0' || parsed_robot_count <= 0) {
    cerr << "Invalid robot count: '" << World::args[1] << "'\n";
    return false;
  }
  num_robots = static_cast<int>(parsed_robot_count);

  const string experiment_mode =
      environment_value("FORMATION_EXPERIMENT_MODE", "fixed_density");
  const string arena_text = environment_value("FORMATION_ARENA_SIDE");
  const formation::ArenaResolution arena = formation::resolve_arena_side(
      static_cast<size_t>(num_robots), experiment_mode, arena_text);
  if (!arena.success) {
    cerr << arena.error << '\n';
    return false;
  }
  xbound = arena.side;
  ybound = arena.side;
  if (arena.qualitative) {
    cerr << "Using qualitative arena side " << xbound
         << "; this run is excluded from fixed-density statistics\n";
  } else if (arena_text.empty()) {
    cerr << "FORMATION_ARENA_SIDE is unset; using fixed-density side " << xbound
         << " for this interactive run\n";
  }

  const string seed_text = environment_value("FORMATION_SEED");
  if (!seed_text.empty() && !parse_unsigned_long(seed_text, &experiment_seed)) {
    cerr << "Invalid FORMATION_SEED: '" << seed_text << "'\n";
    return false;
  }
  unsigned long controller_seed = experiment_seed ^ 0x9e3779b9UL;
  const string controller_seed_text = environment_value("FORMATION_CONTROLLER_SEED");
  if (!controller_seed_text.empty()
      && !parse_unsigned_long(controller_seed_text, &controller_seed)) {
    cerr << "Invalid FORMATION_CONTROLLER_SEED: '" << controller_seed_text << "'\n";
    return false;
  }
  std::seed_seq seed_sequence { static_cast<unsigned int>(controller_seed),
                                static_cast<unsigned int>(controller_seed >> 32), 0xa341316cU };
  controller_rng.seed(seed_sequence);

  const string timeout_text = environment_value("FORMATION_SIM_TIMEOUT");
  if (!timeout_text.empty() && !parse_positive_double(timeout_text, &simulation_timeout_seconds)) {
    cerr << "Invalid FORMATION_SIM_TIMEOUT: '" << timeout_text << "'\n";
    return false;
  }

  const string progress_interval_text = environment_value("FORMATION_PROGRESS_INTERVAL");
  if (!progress_interval_text.empty()
      && !parse_positive_double(progress_interval_text, &progress_interval_seconds)) {
    cerr << "Invalid FORMATION_PROGRESS_INTERVAL: '" << progress_interval_text << "'\n";
    return false;
  }
  next_progress_report_seconds = progress_interval_seconds;

  const string save_image_text = environment_value("FORMATION_SAVE_GENERATED_IMAGE");
  if (!save_image_text.empty()) {
    if (save_image_text == "0")
      save_generated_img = false;
    else if (save_image_text == "1")
      save_generated_img = true;
    else {
      cerr << "FORMATION_SAVE_GENERATED_IMAGE must be 0 or 1\n";
      return false;
    }
  }

  (void)world;
  return true;
}

double random_unit()
{
  return std::uniform_real_distribution<double>(0.0, 1.0)(controller_rng);
}

int random_index(size_t count)
{
  if (count == 0)
    return -1;
  return std::uniform_int_distribution<int>(0, static_cast<int>(count - 1))(controller_rng);
}

void finish_run(const string &status, World *world, double max_target_error)
{
  if (result_written || world == NULL)
    return;
  result_written = true;

  vector<formation::PathLengthTracker> trackers;
  trackers.reserve(g_robots.size());
  for (size_t index = 0; index < g_robots.size(); ++index)
    trackers.push_back(g_robots[index]->path_length);
  const double total_distance_m = formation::total_distance_meters(trackers);
  const double mean_distance_m = num_robots > 0 ? total_distance_m / num_robots : 0.0;
  const double simulation_time = static_cast<double>(world->SimTimeNow()) / 1e6;

  const string input_path = World::args.size() > 2 ? World::args[2] : string();
  const string input_filename = get_img_name(input_path, "/", 1);
  const string default_image_id = get_img_name(input_filename, ".", 0);
  const string run_id = environment_value("FORMATION_RUN_ID", "interactive-" + to_string(num_robots)
                                                                  + "-" + default_image_id);
  const string image_id = environment_value("FORMATION_IMAGE_ID", default_image_id);
  const string image_sha256 = environment_value("FORMATION_IMAGE_SHA256");
  const string result_path = environment_value("FORMATION_RESULT_PATH");

  ostringstream json;
  json << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"run_id\": \"" << json_escape(run_id) << "\",\n"
       << "  \"N\": " << num_robots << ",\n"
       << "  \"image_id\": \"" << json_escape(image_id) << "\",\n"
       << "  \"image_sha256\": \"" << json_escape(image_sha256) << "\",\n"
       << "  \"seed\": " << experiment_seed << ",\n"
       << "  \"arena_side\": ";
  write_json_number(json, xbound);
  json << ",\n  \"status\": \"" << json_escape(status) << "\",\n"
       << "  \"convergence_time_s\": ";
  if (status == "success")
    write_json_number(json, simulation_time);
  else
    json << "null";
  json << ",\n  \"total_distance_m\": ";
  write_json_number(json, total_distance_m);
  json << ",\n  \"mean_distance_per_robot_m\": ";
  write_json_number(json, mean_distance_m);
  json << ",\n  \"target_min_distance\": ";
  write_json_number(json, grid_length);
  json << ",\n  \"target_mst_bottleneck\": ";
  write_json_number(json, target_mst_bottleneck);
  json << ",\n  \"final_max_target_error\": ";
  write_json_number(json, max_target_error);
  json << "\n}\n";

  if (result_path.empty()) {
    cout << json.str();
  } else {
    const string temporary_path = result_path + ".tmp";
    ofstream output(temporary_path.c_str(), ios::out | ios::trunc);
    if (!output) {
      cerr << "Unable to open result file '" << temporary_path << "'\n";
    } else {
      output << json.str();
      output.close();
      if (std::rename(temporary_path.c_str(), result_path.c_str()) != 0)
        cerr << "Unable to publish result file '" << result_path << "'\n";
    }
  }

  cout << "formation run " << run_id << " finished with status=" << status << " at "
       << simulation_time << " simulated seconds\n";
  world->Quit();
}

int ExperimentUpdate(World *world, void *user)
{
  (void)user;
  if (result_written)
    return 1;
  if (g_robots.size() != static_cast<size_t>(num_robots))
    return 0;

  for (size_t index = 0; index < g_robots.size(); ++index) {
    const Pose pose = g_robots[index]->position->GetPose();
    g_robots[index]->path_length.observe({ pose.x, pose.y });
  }

  const formation::CompletionResult completion = current_completion();
  if (completion.complete) {
    finish_run("success", world, completion.max_target_error);
    return 1;
  }

  const double simulation_time = static_cast<double>(world->SimTimeNow()) / 1e6;
  if (simulation_time >= next_progress_report_seconds) {
    cout << "formation progress run_id=" << environment_value("FORMATION_RUN_ID", "interactive")
         << " N=" << num_robots << " simulated_time_s=" << simulation_time
         << " max_target_error=" << completion.max_target_error << endl;
    do {
      next_progress_report_seconds += progress_interval_seconds;
    } while (next_progress_report_seconds <= simulation_time);
  }
  if (simulation_time >= simulation_timeout_seconds) {
    finish_run("sim_timeout", world, completion.max_target_error);
    return 1;
  }
  return 0;
}

// Stage calls this when the model starts up
extern "C" int Init(Model *mod)
{
  World *world = mod->GetWorld();
  if (!initialize_experiment_settings(world)) {
    finish_run("runtime_error", world, std::numeric_limits<double>::infinity());
    return -1;
  }
  string data_path = World::args[2];

  // All robot controllers share one immutable target set. Generate or load it
  // once before registering callbacks so a failed initialization cannot leave
  // a controller running with an empty target vector.
  if (!targets_initialized) {
    targets_initialized = true;
    const bool text_input =
        data_path.size() >= 4 && data_path.compare(data_path.size() - 4, 4, ".txt") == 0;
    if (use_img && !text_input) {
      // grid_length = generate_targets(g_targets, data_path);
      grid_length = generate_targets_new(g_targets, data_path);
    } else {
      grid_length = read_data(g_targets, data_path);
    }
  }
  if (!std::isfinite(grid_length) || grid_length <= 0.0
      || g_targets.size() != static_cast<size_t>(num_robots)) {
    finish_run("target_generation_failed", world, std::numeric_limits<double>::infinity());
    return -1;
  }
  if (target_spatial_index.size() != g_targets.size()) {
    vector<formation::TargetPoint2D> target_points;
    target_points.reserve(g_targets.size());
    for (size_t index = 0; index < g_targets.size(); ++index)
      target_points.push_back(formation::TargetPoint2D(g_targets[index].x, g_targets[index].y));
    target_spatial_index.rebuild(target_points);
  }

  robot_t *robot = new robot_t;
  robot->position = (ModelPosition *)mod;

  robot->fiducial = (ModelFiducial *)mod->GetUnusedModelOfType("fiducial");
  assert(robot->fiducial);
  robot->fiducial->AddCallback(Model::CB_UPDATE, (model_callback_t)FiducialUpdate, robot);

  robot->fiducial->Subscribe();
  robot->position->Subscribe();

  Pose pose = robot->position->GetPose();
  robot->path_length.observe({ pose.x, pose.y });
  g_robots.push_back(robot);
  if (!world_callback_registered) {
    world->AddUpdateCallback(ExperimentUpdate, NULL);
    world_callback_registered = true;
  }
  double dist = 1e6;
  if (grid_length > 0) { // generate targets successfully
    InitMode mode = RAND_INIT; // RAND_INIT or CLOEST_INIT
    int init_idx = 0;
    switch (mode) {
    case RAND_INIT: // random assign a target
      init_idx = random_index(static_cast<size_t>(num_robots));
      break;
    case CLOEST_INIT: // assign closest target
      for (int i = 0; i < num_robots; ++i) {
        if (compute_dist(pose, g_targets[i]) < dist) {
          dist = compute_dist(pose, g_targets[i]);
          init_idx = i;
        }
      }
      break;
    default: break;
    }

    robot->position->setTargetIdx(init_idx);
    robot->known_assigned_target_idx.push_back(init_idx);
    int candidate_target_idx = random_index(static_cast<size_t>(num_robots));
    robot->position->setCandidateTargetIdx(candidate_target_idx);
    robot->position->setNearestUnoccupiedTargetIdx(-1); // init as -1
  }

  return 0; // ok
}

int FiducialUpdate(ModelFiducial *fid, robot_t *robot)
{
  Pose pose = robot->position->GetPose();
  int target_idx = robot->position->getTargetIdx();
  int candidate_target_idx = robot->position->getCandidateTargetIdx();
  int hop = robot->position->getHop();
  if (target_idx < 0 || target_idx >= num_robots || candidate_target_idx < 0
      || candidate_target_idx >= num_robots) {
    finish_run("runtime_error", robot->position->GetWorld(),
               std::numeric_limits<double>::infinity());
    return -1;
  }
  Pose target_pose = g_targets[target_idx];

  // check detections
  std::vector<ModelFiducial::Fiducial> &fids = fid->GetFiducials();
  // no robots in detection range, move to target directly
  if (fids.size() == 0) {
    // position control
    robot->position->GoTo(target_pose); // diff 1: no next step, just go to target
    // robot->position->Say(to_string(target_idx) + ',' + to_string(candidate_target_idx) + ',' +
    // to_string(hop));
    robot->position->Say(to_string(hop));
    return 0;
  }

  // exists robots in detection range, loop through all msg in buffer
  robot->detection = NULL;
  vector<int> all_neighbor_target_idx;
  // store its own target
  if (find(robot->known_assigned_target_idx.begin(), robot->known_assigned_target_idx.end(),
           target_idx)
      == robot->known_assigned_target_idx.end()) {
    robot->known_assigned_target_idx.push_back(target_idx);
  }
  FOR_EACH (it, fid->GetFiducials()) {
    ModelFiducial::Fiducial *neighbor = &(*it);
    // Pose neighbor_pose = neighbor->mod->GetPose();
    int neighbor_target_idx = neighbor->mod->getTargetIdx();
    all_neighbor_target_idx.push_back(neighbor_target_idx);
    int neighbor_candidate_target_idx = neighbor->mod->getCandidateTargetIdx();
    int neighbor_hop = neighbor->mod->getHop();
    if (neighbor_hop < hop) {
      hop = neighbor_hop;
      candidate_target_idx = neighbor_candidate_target_idx;
    }
    // update known assigned targets
    if (find(robot->known_assigned_target_idx.begin(), robot->known_assigned_target_idx.end(),
             neighbor_target_idx)
        == robot->known_assigned_target_idx.end()) {
      robot->known_assigned_target_idx.push_back(neighbor_target_idx);
    }
  }
  hop += 1; // transmit min_neighbor_hop+1

  // decide roles (anchoring or common)
  // diff 2: use target in detection range instead of surroundings
  vector<int> tgt_in_range = get_targets_in_range(pose);
  for (int i = 0; i < tgt_in_range.size(); ++i) {
    // if neighbor picks this target
    bool cond1 =
        find(all_neighbor_target_idx.begin(), all_neighbor_target_idx.end(), tgt_in_range[i])
        == all_neighbor_target_idx.end();
    // if I know this target is assigned
    bool cond2 = find(robot->known_assigned_target_idx.begin(),
                      robot->known_assigned_target_idx.end(), tgt_in_range[i])
                 == robot->known_assigned_target_idx.end();
    if (cond1 && cond2) {
      candidate_target_idx = tgt_in_range[i];
      hop = 0;
      break;
    }
  }

  // diff 2: directly use target one step away instead of surroundings
  // vector<int> tgt_one_step = get_targets_one_step_away(pose);
  // for (int i = 0; i < tgt_one_step.size(); ++i) {
  //   bool cond =
  //       find(all_neighbor_target_idx.begin(), all_neighbor_target_idx.end(), tgt_one_step[i])
  //       == all_neighbor_target_idx.end();
  //   if (cond && tgt_one_step[i] != target_idx) {
  //     candidate_target_idx = tgt_one_step[i];
  //     hop = 0;
  //     break;
  //   }
  // }
  //
  // int ind = nearest_unassigned_target_in_detection_range(robot->known_assigned_target_idx, pose);
  // bool cond = find(all_neighbor_target_idx.begin(), all_neighbor_target_idx.end(), ind)
  //             == all_neighbor_target_idx.end();
  // if (ind != -1 && cond) { // there exists empty target position
  //   candidate_target_idx = ind;
  //   hop = 0;
  // }

  // update candidate target & hop
  robot->position->setCandidateTargetIdx(candidate_target_idx);
  robot->position->setHop(hop);
  // robot->position->Say(to_string(target_idx) + ',' + to_string(candidate_target_idx) + ','
  // + to_string(hop));
  robot->position->Say(to_string(hop));
  if (show_debug_info) {
    if (hop == 0) { // set anchoring robot as red
      robot->position->SetColor(Color(1, 0, 0));
    } else {
      robot->position->SetColor(Color(0, 0, 0));
    }
  }

  FOR_EACH (it, fid->GetFiducials()) {
    ModelFiducial::Fiducial *neighbor = &(*it);
    Pose neighbor_pose = neighbor->mod->GetPose();
    int neighbor_target_idx = neighbor->mod->getTargetIdx();
    // case 1: target confliction, choose candidate or random unassigned target
    if (neighbor_target_idx == target_idx) {
      if ((pose.Distance(target_pose) > neighbor_pose.Distance(target_pose))
          || (pose.Distance(target_pose) == neighbor_pose.Distance(target_pose)
              && is_lexical_small(pose,
                                  neighbor_pose))) { // diff 3: by distance instead of coordinates
        if (random_unit() > 0.1) {
          target_idx = candidate_target_idx;
        } else { // diff 4: random unassigned not random from all targets
          target_idx = random_unassigned_target(robot->known_assigned_target_idx);
        }
        target_pose = g_targets[target_idx];
      }
    }
    // case 2: swap
    if (use_swap) {
      bool reduce_cost =
          target_swap_reduce_cost(pose, target_pose, neighbor_pose, g_targets[neighbor_target_idx]);
      // if (reduce_cost) {
      if (reduce_cost && random_unit() < 0.8) {
        int tmp = target_idx;
        target_idx = neighbor_target_idx;
        neighbor_target_idx = tmp;
        target_pose = g_targets[target_idx];
      }
      // update target
      robot->position->setTargetIdx(target_idx);
      neighbor->mod->setTargetIdx(neighbor_target_idx);
      robot->position->setCandidateTargetIdx(candidate_target_idx);
    }
  }

  // motion planner, update pose
  // avoid collision first
  Pose tmp;
  tmp.x = 0;
  tmp.y = 0;
  const auto add_collision_avoidance = [&](const ModelFiducial::Fiducial &detected_robot) {
    const formation::AvoidanceVector avoidance = formation::collision_avoidance_vector(
        detected_robot.range, detected_robot.bearing, pose.a, SAFE_DIST);
    tmp.x += avoidance.x;
    tmp.y += avoidance.y;
  };
  AvoidCollisionMode avoid_collision_mode = AVOID_ALL;
  switch (avoid_collision_mode) {
  case AVOID_ALL: {
    for (unsigned int i = 0; i < fids.size(); ++i) {
      add_collision_avoidance(fids[i]);
    }
  } break;
  case AVOID_CLOSEST: {
    double closest_dist = 1e6;
    const ModelFiducial::Fiducial *closest = nullptr;
    for (unsigned int i = 0; i < fids.size(); ++i) {
      const ModelFiducial::Fiducial &detected_robot = fids[i];
      if (detected_robot.range < closest_dist) {
        closest_dist = detected_robot.range;
        closest = &detected_robot;
      }
    }
    if (closest != nullptr) {
      add_collision_avoidance(*closest);
    }
  } break;
  case AVOID_NONE: break;
  default: printf("Avoid collision mode not set\n"); break;
  }

  // position control. target_idx may have changed during conflict resolution.
  target_pose = g_targets[target_idx];
  target_pose.x += tmp.x;
  target_pose.y += tmp.y;
  robot->position->GoTo(target_pose);
  // robot->position->Say(to_string(target_idx) + ',' + to_string(candidate_target_idx) + ','
  //                      + to_string(hop));
  robot->position->Say(to_string(hop));
  return 0;
}

bool target_swap_reduce_cost(Pose p1, Pose t1, Pose p2, Pose t2)
{
  double dist1 = compute_dist(p1, t1);
  double dist2 = compute_dist(p2, t2);
  double dist3 = compute_dist(p1, t2);
  double dist4 = compute_dist(p2, t1);
  // swap
  if (dist3 + dist4 < dist1 + dist2) {
    return true;
  } else
    return false;
}

vector<int> get_unassigned_target_idx(vector<int> assigned)
{
  vector<int> all;
  for (int i = 0; i < num_robots; ++i)
    all.push_back(i);

  sort(assigned.begin(), assigned.end());

  // vector<int> unassigned(num_robots);
  // vector<int>::iterator it;

  // it = set_difference(all.begin(), all.end(), assigned.begin(), assigned.end(),
  // unassigned.begin()); unassigned.resize(it - unassigned.begin());
  // return unassigned;
  vector<int> diff;
  set_difference(all.begin(), all.end(), assigned.begin(), assigned.end(),
                 inserter(diff, diff.begin()));

  // remove duplicated zeros
  set<int> s(diff.begin(), diff.end());
  diff.assign(s.begin(), s.end());
  // sort(diff.begin(), diff.end());
  return diff;
}

vector<int> get_targets_in_range(Pose pose)
{
  const vector<size_t> nearby = target_spatial_index.query_radius(
      formation::TargetPoint2D(pose.x, pose.y), sensor_range);
  return vector<int>(nearby.begin(), nearby.end());
}

vector<int> unassigned_targets_in_detection_range(vector<int> assigned, Pose pose)
{
  vector<int> unassigned = get_unassigned_target_idx(assigned);
  vector<int> tir = get_targets_in_range(pose);
  sort(tir.begin(), tir.end());

  vector<int> v;
  set_intersection(unassigned.begin(), unassigned.end(), tir.begin(), tir.end(), back_inserter(v));
  return v;
}

vector<int> get_targets_one_step_away(Pose pose)
{
  vector<int> ret;
  for (int i = 0; i < g_targets.size(); ++i) {
    if (compute_dist(pose, g_targets[i]) <= grid_length) {
      ret.push_back(i);
    }
  }
  return ret;
}

int nearest_unassigned_target_in_detection_range(vector<int> assigned, Pose pose)
{
  vector<int> unassigned = get_unassigned_target_idx(assigned);
  vector<int> tir = get_targets_in_range(pose);
  sort(tir.begin(), tir.end());

  // compute set intersection
  vector<int> v;
  set_intersection(unassigned.begin(), unassigned.end(), tir.begin(), tir.end(), back_inserter(v));
  if (v.empty())
    return -1;
  else {
    double min_dist = 1e6;
    int min_dist_target_idx = -1;
    for (unsigned int i = 0; i < v.size(); ++i) {
      double tmp = compute_dist(pose, g_targets[v[i]]);
      if (tmp < min_dist) {
        min_dist = tmp;
        min_dist_target_idx = v[i];
      }
    }
    return min_dist_target_idx;
  }
}

int nearest_unassigned_target(vector<int> assigned, Pose pose)
{
  vector<int> unassigned = get_unassigned_target_idx(assigned);
  double min_dist = 1e6; // big enough
  int target_idx = unassigned[0];

  for (unsigned int i = 0; i < unassigned.size(); ++i) {
    Pose target_pose = g_targets[unassigned[i]];
    double tmp = compute_dist(pose, target_pose);
    if (tmp < min_dist) {
      min_dist = tmp;
      target_idx = unassigned[i];
    }
  }
  return target_idx;
}

int random_unassigned_target(vector<int> assigned)
{
  vector<int> unassigned = get_unassigned_target_idx(assigned);
  const int selected = random_index(unassigned.size());
  return selected < 0 ? random_index(static_cast<size_t>(num_robots)) : unassigned[selected];
}

double compute_dist(Pose p1, Pose p2)
{
  double delta_x = p1.x - p2.x;
  double delta_y = p1.y - p2.y;
  return hypot(delta_x, delta_y);
}

Pose update_pose(Pose pose, Pose target_pose)
{
  double delta_x = target_pose.x - pose.x;
  double delta_y = target_pose.y - pose.y;
  double theta = atan(fabs(delta_y) / fabs(delta_x));

  double update_x = (delta_x > 0) ? (VSPEED * cos(theta)) : (-1.0 * VSPEED * cos(theta));
  double update_y = (delta_y > 0) ? (VSPEED * sin(theta)) : (-1.0 * VSPEED * sin(theta));

  Pose new_pose;
  new_pose.x = pose.x + update_x;
  new_pose.y = pose.y + update_y;
  return new_pose;
}

bool is_lexical_small(Pose p1, Pose p2)
{
  if ((p1.x < p2.x) || (p1.x == p2.x && p1.y < p2.y))
    return true;
  return false;
}

string get_img_name(string s, string delim, int x)
{
  size_t pos = 0;
  string token = s.substr(0, s.find(delim));
  while ((pos = s.find(delim)) != std::string::npos) {
    token = s.substr(0, pos);
    s.erase(0, pos + delim.length());
  }
  if (x == 0)
    return token;
  else
    return s;
}

int a_in_b(Pose a, vector<Pose> b)
{
  for (int i = 0; i < b.size(); ++i) {
    if (fabs(a.x - b[i].x) < 1e-2 && fabs(a.y - b[i].y) < 1e-2) {
      return i;
    }
  }
  return -1;
}

double read_data(vector<Pose> &targets, string filename)
{
  std::ifstream fin(filename);
  int size;
  fin >> size;
  Pose tmp;
  while (fin >> tmp.x && fin >> tmp.y) {
    targets.push_back(tmp);
  }
  targets.resize(num_robots);
  // get target dist (grid length)
  double min_target_dist = 1e6;
  for (int i = 0; i < num_robots; ++i) {
    for (int j = 0; j < num_robots && j != i; ++j) {
      double target_dist = compute_dist(targets[i], targets[j]);
      if (target_dist < min_target_dist) {
        min_target_dist = target_dist;
      }
    }
  }
  return min_target_dist;
}

double generate_targets(vector<Pose> &targets, string img_path)
{
  Mat img = imread(img_path, IMREAD_GRAYSCALE); // height: img.rows, width: img.cols
  threshold(img, img, 100, 255, THRESH_BINARY); // binary image
  int num_black_pixels =
      countNonZero(img == 0); // number of black pixels (zeros), to generate target points
  if (num_black_pixels == 0) {
    printf("ERROR MSG: bad image, num of black pixels equals 0\n");
    return -1.0;
  }
  double scale = floor(1.0 * num_black_pixels / num_robots);
  scale = 1.0 / sqrt(scale);

  Mat scaled_img;
  try {
    resize(img, scaled_img, cv::Size(), scale, scale, INTER_LINEAR_EXACT);
  } catch (Exception &e) {
    // const char* err_msg = e.what();
    cout << "exception caught: " << e.what() << endl;
    cout << "scaled img size: " << scaled_img.cols << '\t' << scaled_img.rows << endl;
    return -1.0;
  }
  // resize(img, scaled_img, cv::Size(), scale, scale, INTER_LINEAR_EXACT);
  // find a proper scale
  while (countNonZero(scaled_img == 0) < num_robots) {
    scale += 1e-4; // if can't make the condition equal, 1e-5
    resize(img, scaled_img, cv::Size(), scale, scale, INTER_LINEAR_EXACT);
  }
  int cnt = 0;
  for (int i = 0; i < scaled_img.rows; ++i) { // img height, y-axis
    for (int j = 0; j < scaled_img.cols; ++j) { // img width, x-axis
      if (scaled_img.at<schar>(i, j) == 0 && cnt < num_robots) {
        Pose tmp;
        // scale to map boundrary first, then translate
        tmp.x = 1.0 * j * xbound / scaled_img.cols - xbound / 2.0;
        tmp.y = 1.0 * (scaled_img.rows - i) * ybound / scaled_img.rows - ybound / 2.0;
        targets.push_back(tmp);
        cnt++;
      }
    }
  }
  targets.resize(num_robots);
  // get target dist (grid length)
  double min_target_dist = 1e6;
  for (int i = 0; i < num_robots; ++i) {
    for (int j = 0; j < num_robots && j != i; ++j) {
      double target_dist = compute_dist(targets[i], targets[j]);
      if (target_dist < min_target_dist) {
        min_target_dist = target_dist;
      }
    }
  }
  /*
    bool scale_smaller = (min_target_dist <= SAFE_DIST);
    bool scale_larger = (min_target_dist >= 0.5 * sensor_range);
    while (scale_smaller || scale_larger || (countNonZero(scaled_img == 0) < num_robots)) {
      if (scale_smaller) {
        // cout << "target dist " << min_target_dist << " < safe dist, re-generating targets...\n";
        scale -= 1e-5;
      } else {
        // cout << "target dist " << min_target_dist << " > sensor range, re-generating
    targets...\n"; scale += 1e-5;
      }
      resize(img, scaled_img, cv::Size(), scale, scale, INTER_LINEAR_EXACT);
      cnt = 0;
      targets.clear();
      for (int i = 0; i < scaled_img.rows; ++i) { // img height, y-axis
        for (int j = 0; j < scaled_img.cols; ++j) { // img width, x-axis
          if (scaled_img.at<schar>(i, j) == 0 && cnt < num_robots) {
            Pose tmp;
            // scale to map boundrary first, then translate
            tmp.x = 1.0 * j * xbound / scaled_img.cols - xbound / 2.0;
            tmp.y = 1.0 * (scaled_img.rows - i) * ybound / scaled_img.rows - ybound / 2.0;
            targets.push_back(tmp);
            cnt++;
          }
        }
      }
      if (targets.size() < num_robots) {
        string test_fname = "/Users/yang/repos/swarm_formation/stage4/Stage/formation/img_gen_"
                            + to_string(num_robots) + ".txt";
        string img_name = get_img_name(img_path, "/", 1);
        std::ofstream myfile;
        myfile.open(test_fname, ios::app);
        myfile << img_name << '\t' << "fail: generated targets size < num robots" << '\n';
        myfile.close();
        return -1.0;
      }
      targets.resize(num_robots);
      min_target_dist = 1e6;
      for (int i = 0; i < num_robots; ++i) {
        for (int j = 0; j < num_robots && j != i; ++j) {
          double target_dist = compute_dist(targets[i], targets[j]);
          if (target_dist < min_target_dist) {
            min_target_dist = target_dist;
          }
        }
      }
    }
  */
  if (show_target_info) {
    double x_min = 1e6;
    double x_max = -1e6;
    double y_min = 1e6;
    double y_max = -1e6;
    for (int i = 0; i < cnt; ++i) {
      if (targets[i].x < x_min)
        x_min = targets[i].x;
      if (targets[i].y < y_min)
        y_min = targets[i].y;
      if (targets[i].x > x_max)
        x_max = targets[i].x;
      if (targets[i].y > y_max)
        y_max = targets[i].y;
    }
    printf("xmin: %2.f\txmax: %.2f\n", x_min, x_max);
    printf("ymin: %2.f\tymax: %.2f\n", y_min, y_max);
    // show_img(scaled_img);
    cout << cnt << endl;
    for (int i = 0; i < cnt; ++i)
      printf("%d %.2f %.2f\n", i, targets[i].x, targets[i].y);
  }

  // Paths are relative to the documented launch directory:
  // Stage/worlds/experiment/formation_world.
  const string formation_output_dir = "../../../formation";
  if (save_generated_img) {
    const string img_name = get_img_name(img_path, "/", 1);
    const string img_save_path =
        formation_output_dir + "/generated_img/" + to_string(num_robots) + '_' + img_name;
    if (!imwrite(img_save_path, scaled_img)) {
      cerr << "Failed to save generated target image: " << img_save_path << '\n';
    }
  }

  // cout << min_target_dist << endl;
  const string test_fname = formation_output_dir + "/img_gen_" + to_string(num_robots) + ".txt";
  const string img_name = get_img_name(img_path, "/", 1);
  std::ofstream myfile;
  myfile.open(test_fname, ios::app);
  bool c1 = min_target_dist <= SAFE_DIST;
  bool c2 = min_target_dist >= 0.5 * sensor_range;
  if (c1) {
    myfile << img_name << '\t' << "small" << '\n';
  }
  if (c2) {
    myfile << img_name << '\t' << "large" << '\n';
  }
  if (!c1 && !c2) {
    myfile << img_name << '\t' << min_target_dist << '\n';
  }
  myfile.close();
  // cout << min_target_dist << endl;
  return min_target_dist;
}

double generate_targets_new(vector<Pose> &targets, string img_path)
{
  targets.clear();
  formation::TargetImageGenerationConfig config;
  config.target_count = static_cast<size_t>(num_robots);
  config.safe_distance = 1.05 * SAFE_DIST;
  config.communication_distance = 0.95 * sensor_range;
  config.arena_width = xbound;
  config.arena_height = ybound;
  config.boundary_margin = radius;

  const formation::TargetImageGenerationResult generated =
      formation::generate_targets_from_image(img_path, config);
  if (!generated.success) {
    cerr << "Target generation failed for '" << img_path << "': " << generated.error << '\n';
    return -1.0;
  }
  const vector<vector<formation::TargetPoint2D> > &component_candidates =
      generated.component_candidates;
  target_mst_bottleneck = generated.metrics.mst_bottleneck;
  if (generated.targets.size() != static_cast<size_t>(num_robots)) {
    cerr << "Target generation failed: expected " << num_robots << " targets, got "
         << generated.targets.size() << '\n';
    return -1.0;
  }

  targets.reserve(generated.targets.size());
  for (size_t index = 0; index < generated.targets.size(); ++index) {
    const formation::TargetPoint2D &point = generated.targets[index].position;
    targets.push_back(Pose(point.x, point.y, 0.0, 0.0));
  }

  cout << "Generated " << targets.size() << " connected targets from "
       << generated.metrics.component_count
       << " foreground component(s): min_distance=" << generated.metrics.minimum_distance
       << ", mst_bottleneck=" << generated.metrics.mst_bottleneck
       << ", scale=" << generated.metrics.scale
       << ", graph_components=" << generated.metrics.graph_component_count << '\n';

  if (save_generated_img) {
    const int canvas_size = 1000;
    const int padding = 40;
    Mat diagnostic(canvas_size, canvas_size, CV_8UC3, Scalar(255, 255, 255));
    const auto to_pixel = [&](const formation::TargetPoint2D &point) {
      const double x_fraction = (point.x + 0.5 * xbound) / xbound;
      const double y_fraction = (0.5 * ybound - point.y) / ybound;
      return Point(padding + cvRound(x_fraction * (canvas_size - 2 * padding)),
                   padding + cvRound(y_fraction * (canvas_size - 2 * padding)));
    };
    const Scalar palette[] = { Scalar(31, 119, 180),  Scalar(255, 127, 14),  Scalar(44, 160, 44),
                               Scalar(214, 39, 40),   Scalar(148, 103, 189), Scalar(140, 86, 75),
                               Scalar(227, 119, 194), Scalar(127, 127, 127) };
    const size_t palette_size = sizeof(palette) / sizeof(palette[0]);

    rectangle(diagnostic, Point(padding, padding),
              Point(canvas_size - padding, canvas_size - padding), Scalar(210, 210, 210), 1);
    const formation::ConnectedTarget &reference = generated.targets.front();
    const formation::TargetPoint2D &reference_translation =
        generated.component_translations[reference.component];
    const double layout_center_x = reference.source.x + reference_translation.x
                                   - reference.position.x / generated.metrics.scale;
    const double layout_center_y = reference.source.y + reference_translation.y
                                   + reference.position.y / generated.metrics.scale;
    for (size_t component = 0; component < component_candidates.size(); ++component) {
      const formation::TargetPoint2D &translation = generated.component_translations[component];
      const Scalar &color = palette[component % palette_size];
      const Scalar light_color(0.35 * color[0] + 0.65 * 255.0, 0.35 * color[1] + 0.65 * 255.0,
                               0.35 * color[2] + 0.65 * 255.0);
      for (size_t index = 0; index < component_candidates[component].size(); ++index) {
        const formation::TargetPoint2D &source = component_candidates[component][index];
        const formation::TargetPoint2D world(
            (source.x + translation.x - layout_center_x) * generated.metrics.scale,
            -(source.y + translation.y - layout_center_y) * generated.metrics.scale);
        circle(diagnostic, to_pixel(world), 1, light_color, FILLED, LINE_AA);
      }
    }
    for (size_t index = 0; index < generated.mst_edges.size(); ++index) {
      const pair<size_t, size_t> &edge = generated.mst_edges[index];
      line(diagnostic, to_pixel(generated.targets[edge.first].position),
           to_pixel(generated.targets[edge.second].position), Scalar(190, 190, 190), 1, LINE_AA);
    }
    for (size_t index = 0; index < generated.targets.size(); ++index) {
      const size_t component = generated.targets[index].component;
      circle(diagnostic, to_pixel(generated.targets[index].position), 5,
             palette[component % palette_size], FILLED, LINE_AA);
    }

    const string summary = "N=" + to_string(num_robots)
                           + " components=" + to_string(generated.metrics.component_count)
                           + " min=" + format("%.3f", generated.metrics.minimum_distance)
                           + " mst=" + format("%.3f", generated.metrics.mst_bottleneck);
    putText(diagnostic, summary, Point(padding, 25), FONT_HERSHEY_SIMPLEX, 0.55, Scalar(30, 30, 30),
            1, LINE_AA);

    const string image_filename = get_img_name(img_path, "/", 1);
    const string image_stem = get_img_name(image_filename, ".", 0);
    const string output_path =
        "../../../formation/generated_img/" + to_string(num_robots) + '_' + image_stem + "_new.png";
    if (!imwrite(output_path, diagnostic)) {
      cerr << "Failed to save generated target diagnostic: " << output_path << '\n';
    } else {
      cout << "Saved generated target diagnostic: " << output_path << '\n';
    }
  }

  return generated.metrics.minimum_distance;
}
