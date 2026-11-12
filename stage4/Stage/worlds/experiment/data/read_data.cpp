#include <iostream>
#include <fstream>
#include <vector>
#include <string.h>
#include <cstdlib>
#include <cmath>

using namespace std;

typedef struct {
  double x;
  double y;
} Pose;

int num_robots = 50;
vector<Pose> g_targets;

double compute_dist(Pose p1, Pose p2)
{
  double delta_x = p1.x - p2.x;
  double delta_y = p1.y - p2.y;
  return hypot(delta_x, delta_y);
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

int main()
{
  string fname = "./map_50_0.txt";
  double len = read_data(g_targets, fname);
  cout << g_targets.size() << endl;
  for (int i = 0; i < g_targets.size(); ++i)
    cout << g_targets[i].x << "," << g_targets[i].y << endl;
  cout << "grid length: " << len << endl;
  return 0;
}
