#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#pragma execution_character_set("utf-8")
#define _CRT_SECURE_NO_WARNINGS

#include <graphics.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <queue>
#include <tchar.h>

// --- UCRT MinGW 兼容补丁开始 ---
#ifdef __cplusplus
extern "C" {
#endif
    FILE* __cdecl __iob_func(void) {
        static FILE* mock_iob[3] = { stdin, stdout, stderr };
        return (FILE*)mock_iob;
    }
    void* __imp___iob_func = (void*)__iob_func;
#ifdef __cplusplus
}
#endif
// --- UCRT MinGW 兼容补丁结束 ---

using namespace std;

#define MAX_VERTEX 50
#define INF 1e9
#define TYPE_FLIGHT 1
#define TYPE_TRAIN 2
#define MAP_AREA_WIDTH (SCREEN_WIDTH - SIDEBAR_WIDTH)
// ----------------- 数据结构定义 -----------------
typedef struct Schedule {
    TCHAR no[20];         // 班次号
    int dep_time;        // 出发时间（分钟）
    int arr_time;        // 到达时间
    double cost;         // 费用
    struct Schedule* next;
} Schedule;

typedef struct ArcNode {
    int adjvex;              // 目标城市下标
    int distance;            // 里程
    int type;                // 1-飞机，2-火车
    Schedule* sch_list;      // 班次链表
    struct ArcNode* nextarc;
} ArcNode;

typedef struct VNode {
    TCHAR name[30];          // 城市名称
    TCHAR code[10];          // 三字代码
    int x, y;                // 地图像素坐标
    ArcNode* firstarc;
} VNode, AdjList[MAX_VERTEX];

typedef struct {
    AdjList vertices;
    int vexnum;
} Graph;

typedef struct {
    double best_value;   // 累积最优值
    int pre_vex;         // 前驱城市
    Schedule* chosen_sch;// 选中的班次
    int arrival_time;    // 到达绝对时间
} PathState;

// ----------------- 全局变量 -----------------
Graph G;
int SCREEN_WIDTH = 800;
int SCREEN_HEIGHT = 600;
int SIDEBAR_WIDTH = 240;
int MAP_AREA_MAX = 560; // 改为非 const 变量
// ----------------- 工具函数 -----------------
int TimeToMin(const TCHAR* time_str) {
    int h = 0, m = 0;
    if (_stscanf_s(time_str, _T("%d:%d"), &h, &m) == 2) {
        return h * 60 + m;
    }
    return 0;
}

// 辅助函数：删除节点（简单实现：标记法或重置法）
void DeleteNode(int node_idx) 
{
    if (node_idx < 0 || node_idx >= G.vexnum) return
;
    // 简单的删除策略：将最后一个节点移到当前位置，更新vexnum
    // 注意：这会改变所有节点的索引，在本项目中可能需要清空当前路径缓存
    G.vexnum--;
    G.vertices[node_idx] = G.vertices[G.vexnum];
    // 清除关联的边(简化处理)
    G.vertices[node_idx].firstarc = 
NULL
; 
}

void MinToTimeStr(int total_min, TCHAR* out_str, size_t size) {
    int days = total_min / 1440;
    int rem = total_min % 1440;
    int h = rem / 60;
    int m = rem % 60;
    if (days == 0) {
        _stprintf_s(out_str, size, _T("当天 %02d:%02d"), h, m);
    }
    else {
        _stprintf_s(out_str, size, _T("第%d天 %02d:%02d"), days + 1, h, m);
    }
}

int LocateVex(const TCHAR* name) {
    for (int i = 0; i < G.vexnum; i++) {
        if (_tcscmp(G.vertices[i].name, name) == 0) return i;
    }
    return -1;
}

int AddVertex(const TCHAR* name, const TCHAR* code, int x, int y) {
    if (G.vexnum >= MAX_VERTEX) return -1;
    // 限制城市坐标在地图区（0-MAP_AREA_MAX, 0-600）
    if (x >= MAP_AREA_MAX) x = MAP_AREA_MAX - 20;
    if (y >= 600) y = 580;
    if (x < 0) x = 20;
    if (y < 0) y = 20;

    int idx = LocateVex(name);
    if (idx != -1) return idx;
    _tcscpy_s(G.vertices[G.vexnum].name, 30, name);
    _tcscpy_s(G.vertices[G.vexnum].code, 10, code);
    G.vertices[G.vexnum].x = x;
    G.vertices[G.vexnum].y = y;
    G.vertices[G.vexnum].firstarc = NULL;
    G.vexnum++;
    return G.vexnum - 1;
}

ArcNode* AddArc(int u, int v, int distance, int type) {
    ArcNode* p = G.vertices[u].firstarc;
    while (p != NULL) {
        if (p->adjvex == v && p->type == type) return p;
        p = p->nextarc;
    }
    ArcNode* arc = (ArcNode*)malloc(sizeof(ArcNode));
    if (!arc) return NULL;
    arc->adjvex = v;
    arc->distance = distance;
    arc->type = type;
    arc->sch_list = NULL;
    arc->nextarc = G.vertices[u].firstarc;
    G.vertices[u].firstarc = arc;
    return arc;
}

void AddSchedule(ArcNode* arc, const TCHAR* no, int dep, int arr, double cost) {
    if (!arc) return;
    Schedule* sch = (Schedule*)malloc(sizeof(Schedule));
    if (!sch) return;
    _tcscpy_s(sch->no, 20, no);
    sch->dep_time = dep;
    sch->arr_time = arr;
    sch->cost = cost;
    sch->next = arc->sch_list;
    arc->sch_list = sch;
}

// ----------------- 初始化测试数据 -----------------
void DeleteVertexByName(const TCHAR* name) {
    int idx = LocateVex(name);
    if (idx == -1) {
        MessageBox(GetHWnd(), _T("未找到该城市！"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // 1. 删除所有指向该节点的边
    for (int i = 0; i < G.vexnum; i++) {
        ArcNode** p = &G.vertices[i].firstarc;
        while (*p != NULL) {
            if ((*p)->adjvex == idx) {
                ArcNode* temp = *p;
                *p = (*p)->nextarc;
                // 这里建议写一个释放 Schedule 链表的函数防止内存泄漏
                free(temp); 
            } else {
                // 如果边的指向索引大于被删除的节点，需要修正索引
                if ((*p)->adjvex > idx) (*p)->adjvex--;
                p = &((*p)->nextarc);
            }
        }
    }

    // 2. 将数组中的最后一个元素移到被删除的位置
    G.vexnum--;
    if (idx < G.vexnum) {
        G.vertices[idx] = G.vertices[G.vexnum];
    }
    
    MessageBox(GetHWnd(), _T("节点删除成功，相关链路已清理。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
}

void InjectTestData() {
    G.vexnum = 0;
    // 所有城市坐标限制在地图区（0-560, 0-600）
    int bj = AddVertex(_T("北京"), _T("BJS"), 500, 160);
    int xa = AddVertex(_T("西安"), _T("SIA"), 400, 280);
    int ur = AddVertex(_T("乌鲁木齐"), _T("URC"), 160, 140);
    int gz = AddVertex(_T("广州"), _T("CAN"), 480, 480);
    int hrb = AddVertex(_T("哈尔滨"), _T("HRB"), 550, 80);
    int nj = AddVertex(_T("南京"), _T("NKG"), 520, 290);
    int sh = AddVertex(_T("上海"), _T("SHA"), 540, 310);
    int cd = AddVertex(_T("成都"), _T("CTU"), 350, 340);
    int wh = AddVertex(_T("武汉"), _T("WUH"), 490, 330);
    int km = AddVertex(_T("昆明"), _T("KMG"), 320, 440);

    // 北京 -> 西安
    ArcNode* a1_f = AddArc(bj, xa, 1200, TYPE_FLIGHT);
    AddSchedule(a1_f, _T("CA1201"), TimeToMin(_T("11:30")), TimeToMin(_T("13:30")), 800);
    AddSchedule(a1_f, _T("CA1202"), TimeToMin(_T("08:00")), TimeToMin(_T("10:00")), 500);
    ArcNode* a1_t = AddArc(bj, xa, 1200, TYPE_TRAIN);
    AddSchedule(a1_t, _T("Z19"), TimeToMin(_T("20:30")), TimeToMin(_T("20:30")) + 480, 230);

    // 西安 -> 乌鲁木齐
    ArcNode* a2_f = AddArc(xa, ur, 2500, TYPE_FLIGHT);
    AddSchedule(a2_f, _T("CA1302"), TimeToMin(_T("14:30")), TimeToMin(_T("18:30")), 1200);
    AddSchedule(a2_f, _T("CA1306"), TimeToMin(_T("16:00")), TimeToMin(_T("20:00")), 1500);
    ArcNode* a2_t = AddArc(xa, ur, 2500, TYPE_TRAIN);
    AddSchedule(a2_t, _T("T53"), TimeToMin(_T("20:00")), TimeToMin(_T("20:00")) + 1200, 320);

    // 广州 -> 哈尔滨
    ArcNode* a3_t = AddArc(gz, hrb, 3500, TYPE_TRAIN);
    AddSchedule(a3_t, _T("T124"), TimeToMin(_T("10:30")), TimeToMin(_T("10:30")) + 1600, 450);
    ArcNode* a3_f = AddArc(gz, hrb, 3500, TYPE_FLIGHT);
    AddSchedule(a3_f, _T("CZ3601"), TimeToMin(_T("08:30")), TimeToMin(_T("12:45")), 1800);

    // 乌鲁木齐 -> 北京
    ArcNode* a4_f = AddArc(ur, bj, 2800, TYPE_FLIGHT);
    AddSchedule(a4_f, _T("HU714"), TimeToMin(_T("13:00")), TimeToMin(_T("17:00")), 600);

    // 北京 -> 南京
    ArcNode* a5_f = AddArc(bj, nj, 1000, TYPE_FLIGHT);
    AddSchedule(a5_f, _T("MU281"), TimeToMin(_T("20:00")), TimeToMin(_T("22:00")), 300);
    ArcNode* a5_t = AddArc(bj, nj, 1000, TYPE_TRAIN);
    AddSchedule(a5_t, _T("G101"), TimeToMin(_T("07:00")), TimeToMin(_T("11:30")), 445);

    // 南京 -> 上海
    ArcNode* a6_t = AddArc(nj, sh, 300, TYPE_TRAIN);
    AddSchedule(a6_t, _T("G7001"), TimeToMin(_T("08:00")), TimeToMin(_T("09:15")), 140);
    AddSchedule(a6_t, _T("G7003"), TimeToMin(_T("13:00")), TimeToMin(_T("14:15")), 140);

    // 成都 -> 西安
    ArcNode* a7_t = AddArc(cd, xa, 650, TYPE_TRAIN);
    AddSchedule(a7_t, _T("D1912"), TimeToMin(_T("09:00")), TimeToMin(_T("12:30")), 263);

    // 昆明 -> 成都
    ArcNode* a8_t = AddArc(km, cd, 850, TYPE_TRAIN);
    AddSchedule(a8_t, _T("G2886"), TimeToMin(_T("10:00")), TimeToMin(_T("15:30")), 340);
}

// ----------------- 核心算法 -----------------
bool SolveSingleLeg(int start, int end, int start_time, bool is_transfer_at_start, int decision, int vehicle, vector<int>& path_out, vector<Schedule*>& sch_out) {
    PathState state[MAX_VERTEX];
    bool in_queue[MAX_VERTEX];
    int queue_arr[MAX_VERTEX * 10];
    int head = 0, tail = 0;

    int min_wait = (vehicle == TYPE_FLIGHT) ? 120 : 60; 

    for (int i = 0; i < G.vexnum; i++) {
        state[i].best_value = INF;
        state[i].pre_vex = -1;
        state[i].chosen_sch = NULL;
        state[i].arrival_time = INF;
        in_queue[i] = false;
    }

    state[start].arrival_time = start_time;
    state[start].best_value = (decision == 1) ? start_time : 0;
    queue_arr[tail++] = start;
    in_queue[start] = true;

    while (head < tail) {
        int u = queue_arr[head++];
        in_queue[u] = false;

        for (ArcNode* arc = G.vertices[u].firstarc; arc != NULL; arc = arc->nextarc) {
            if (arc->type != vehicle) continue;
            int v = arc->adjvex;

            for (Schedule* sch = arc->sch_list; sch != NULL; sch = sch->next) {
                int u_arrival = state[u].arrival_time;
                int buffer = (u == start && !is_transfer_at_start) ? 0 : min_wait;
                int earliest_dep = u_arrival + buffer;

                int day_offset = 0;
                if (sch->dep_time < earliest_dep) {
                    day_offset = ((earliest_dep - sch->dep_time - 1) / 1440 + 1) * 1440;
                }
                int actual_dep = sch->dep_time + day_offset;
                int actual_arr = sch->arr_time + day_offset;

                if (decision == 1) { 
                    if (actual_arr < state[v].best_value) {
                        state[v].best_value = actual_arr;
                        state[v].arrival_time = actual_arr;
                        state[v].pre_vex = u;
                        state[v].chosen_sch = sch;
                        if (!in_queue[v]) {
                            queue_arr[tail++] = v;
                            in_queue[v] = true;
                        }
                    }
                }
                else { 
                    double cost = state[u].best_value + sch->cost;
                    if (cost < state[v].best_value || (cost == state[v].best_value && actual_arr < state[v].arrival_time)) {
                        state[v].best_value = cost;
                        state[v].arrival_time = actual_arr;
                        state[v].pre_vex = u;
                        state[v].chosen_sch = sch;
                        if (!in_queue[v]) {
                            queue_arr[tail++] = v;
                            in_queue[v] = true;
                        }
                    }
                }
            }
        }
    }

    if (state[end].best_value >= INF) return false;

    vector<int> temp_path;
    vector<Schedule*> temp_sch;
    int curr = end;
    while (curr != start) {
        temp_path.push_back(curr);
        temp_sch.push_back(state[curr].chosen_sch);
        curr = state[curr].pre_vex;
    }
    temp_path.push_back(start);

    reverse(temp_path.begin(), temp_path.end());
    reverse(temp_sch.begin(), temp_sch.end());

    path_out = temp_path;
    sch_out = temp_sch;
    return true;
}

struct GlobalResult {
    vector<int> full_path;
    vector<Schedule*> full_schs;
    vector<int> dep_times_absolute;
    vector<int> arr_times_absolute;
    int total_duration;
    double total_cost;
};

GlobalResult SolveWithVias(int start, int end, int start_time, int decision, int vehicle, const vector<int>& vias) {
    GlobalResult best_res;
    best_res.total_duration = INF;
    best_res.total_cost = INF;

    vector<int> perm = vias;
    sort(perm.begin(), perm.end());

    do {
        vector<int> sequence;
        sequence.push_back(start);
        for (size_t i = 0; i < perm.size(); i++) {
            sequence.push_back(perm[i]);
        }
        sequence.push_back(end);

        int current_min = start_time;
        vector<int> combined_path;
        vector<Schedule*> combined_sch;
        vector<int> dep_abs;
        vector<int> arr_abs;
        bool possible = true;

        for (size_t i = 0; i < sequence.size() - 1; i++) {
            int from_node = sequence[i];
            int to_node = sequence[i + 1];
            bool is_transfer = (i > 0); 

            vector<int> sub_path;
            vector<Schedule*> sub_sch;
            if (!SolveSingleLeg(from_node, to_node, current_min, is_transfer, decision, vehicle, sub_path, sub_sch)) {
                possible = false;
                break;
            }

            int u_arrival = current_min;
            int min_wait = (vehicle == TYPE_FLIGHT) ? 120 : 60;
            for (size_t k = 0; k < sub_sch.size(); k++) {
                int buffer = (k == 0 && !is_transfer) ? 0 : min_wait;
                int earliest_dep = u_arrival + buffer;
                int day_offset = 0;
                if (sub_sch[k]->dep_time < earliest_dep) {
                    day_offset = ((earliest_dep - sub_sch[k]->dep_time - 1) / 1440 + 1) * 1440;
                }
                int actual_dep = sub_sch[k]->dep_time + day_offset;
                int actual_arr = sub_sch[k]->arr_time + day_offset;

                dep_abs.push_back(actual_dep);
                arr_abs.push_back(actual_arr);
                u_arrival = actual_arr;
            }

            if (combined_path.empty()) {
                combined_path = sub_path;
            }
            else {
                for (size_t k = 1; k < sub_path.size(); k++) combined_path.push_back(sub_path[k]);
            }
            for (auto* s : sub_sch) combined_sch.push_back(s);

            current_min = u_arrival;
        }

        if (possible) {
            int duration = current_min - start_time;
            double cost = 0;
            for (auto* s : combined_sch) cost += s->cost;

            bool is_better = false;
            if (best_res.total_duration == INF) {
                is_better = true;
            }
            else if (decision == 1) { 
                if (duration < best_res.total_duration) is_better = true;
            }
            else { 
                if (cost < best_res.total_cost) is_better = true;
            }

            if (is_better) {
                best_res.full_path = combined_path;
                best_res.full_schs = combined_sch;
                best_res.dep_times_absolute = dep_abs;
                best_res.arr_times_absolute = arr_abs;
                best_res.total_duration = duration;
                best_res.total_cost = cost;
            }
        }

    } while (next_permutation(perm.begin(), perm.end()));

    return best_res;
}

// ----------------- 交互式数据编辑 -----------------
void InteractiveEditNetwork() {
    HWND hwnd = GetHWnd();
    int msg_choice = MessageBox(hwnd, 
        _T("即将开启图形引导录入。系统会自动检测城市是否存在，若不存在将引导您设定该城市在地图上的显示坐标及三字代码。是否继续？"), 
        _T("智能网络数据管理器"), MB_YESNO | MB_ICONINFORMATION);
        
    if (msg_choice != IDYES) return;

    TCHAR src_name[30] = {0}, dest_name[30] = {0}, no[20] = {0};
    TCHAR dep_str[10] = {0}, arr_str[10] = {0}, vehicle_str[10] = {0};
    TCHAR dist_str[10] = {0}, cost_str[10] = {0};
    TCHAR x_str[10] = {0}, y_str[10] = {0}, code_str[10] = {0};

    // 1. 输入起点城市
    if (!InputBox(src_name, 30, _T("请输入起点城市名称（如：成都）："), _T("添加联动关系 - [1/9] 起点名称"))) return;
    int u = LocateVex(src_name);
    if (u == -1) {
        TCHAR hint[100];
        _stprintf_s(hint, 100, _T("检测到城市 [%s] 尚不存在。请输入其三字英文代码/简称（如：CTU）："), src_name);
        if (!InputBox(code_str, 10, hint, _T("自动创建起点节点 - [1/3] 三字码"))) return;
        
        _stprintf_s(hint, 100, _T("请输入 [%s] 在地图上的 X 像素坐标\n(当前大地图有效范围：0 到 %d)："), src_name, MAP_AREA_MAX-20);
        if (!InputBox(x_str, 10, hint, _T("自动创建起点节点 - [2/3] X坐标"), _T("300"))) return;
        
        _stprintf_s(hint, 100, _T("请输入 [%s] 在地图上的 Y 像素坐标\n(当前大地图有效范围：0 到 600)："), src_name);
        if (!InputBox(y_str, 10, hint, _T("自动创建起点节点 - [3/3] Y坐标"), _T("300"))) return;

        int nx = _ttoi(x_str);
        int ny = _ttoi(y_str);
        u = AddVertex(src_name, code_str, nx, ny);
        if (u == -1) { MessageBox(hwnd, _T("错误：网络图城市节点数量已达上限！"), _T("错误"), MB_OK | MB_ICONERROR); return; }
    }

    // 2. 输入终点城市
    if (!InputBox(dest_name, 30, _T("请输入终点城市名称（如：北京）："), _T("添加联动关系 - [2/9] 终点名称"))) return;
    int v = LocateVex(dest_name);
    if (v == -1) {
        TCHAR hint[100];
        _stprintf_s(hint, 100, _T("检测到城市 [%s] 尚不存在。请输入其三字英文代码/简称（如：BJS）："), dest_name);
        if (!InputBox(code_str, 10, hint, _T("自动创建终点节点 - [1/3] 三字码"))) return;
        
        _stprintf_s(hint, 100, _T("请输入 [%s] 在地图上的 X 像素坐标\n(当前大地图有效范围：0 到 %d)："), dest_name, MAP_AREA_MAX-20);
        if (!InputBox(x_str, 10, hint, _T("自动创建终点节点 - [2/3] X坐标"), _T("400"))) return;
        
        _stprintf_s(hint, 100, _T("请输入 [%s] 在地图上的 Y 像素坐标\n(当前大地图有效范围：0 到 600)："), dest_name);
        if (!InputBox(y_str, 10, hint, _T("自动创建终点节点 - [3/3] Y坐标"), _T("250"))) return;

        int nx = _ttoi(x_str);
        int ny = _ttoi(y_str);
        v = AddVertex(dest_name, code_str, nx, ny);
        if (v == -1) { MessageBox(hwnd, _T("错误：网络图城市节点数量已达上限！"), _T("错误"), MB_OK | MB_ICONERROR); return; }
    }

    if (u == v) {
        MessageBox(hwnd, _T("起点和终点不能为同一城市！"), _T("录入失败"), MB_OK | MB_ICONWARNING);
        return;
    }

    // 3. 交通工具选择
    if (!InputBox(vehicle_str, 10, _T("请输入交通工具类型：\n输入 1 代表 [飞机]\n输入 2 代表 [火车]"), _T("添加联动关系 - [3/9] 载具类型"), _T("1"))) return;
    int vehicle = _ttoi(vehicle_str);
    if (vehicle != 1 && vehicle != 2) {
        MessageBox(hwnd, _T("无效的工具类型！必须输入 1(飞机) 或 2(火车)。"), _T("录入失败"), MB_OK | MB_ICONWARNING);
        return;
    }

    // 4. 里程输入
    if (!InputBox(dist_str, 10, _T("请输入两城市间的里程距离（公里）："), _T("添加联动关系 - [4/9] 里程"), _T("1000"))) return;
    int distance = _ttoi(dist_str);

    // 5. 班次/航班号
    if (!InputBox(no, 20, _T("请输入航班号或列车车次（如：3U8881 / G102）："), _T("添加联动关系 - [5/9] 班次编号"))) return;

    // 6. 出发时间
    if (!InputBox(dep_str, 10, _T("请输入预计出发时间\n格式为 hh:mm（如：08:30）："), _T("添加联动关系 - [6/9] 出发时间"))) return;

    // 7. 到达时间
    if (!InputBox(arr_str, 10, _T("请输入预计到达时间\n格式为 hh:mm（如：11:45）："), _T("添加联动关系 - [7/9] 到达时间"))) return;

    // 8. 价格票价
    if (!InputBox(cost_str, 10, _T("请输入单人标准票价（元）："), _T("添加联动关系 - [8/9] 票价旅资"))) return;
    double cost = _tcstod(cost_str, NULL);

    int dep_min = TimeToMin(dep_str);
    int arr_min = TimeToMin(arr_str);
    if (arr_min < dep_min) {
        arr_min += 1440;
    }

    ArcNode* arc = AddArc(u, v, distance, vehicle);
    AddSchedule(arc, no, dep_min, arr_min, cost);

    TCHAR success_msg[200];
    _stprintf_s(success_msg, 200, _T("【成功】[%s] 经由 [%s] 至 [%s] 的交通运输大通道以及车次 [%s] 已实时成功注入图形网络中！"), src_name, vehicle == 1 ? _T("航空") : _T("铁路"), dest_name, no);
    MessageBox(hwnd, success_msg, _T("数据添加成功"), MB_OK | MB_ICONINFORMATION);
}

// ----------------- UI渲染工具函数 -----------------
bool IsInCircle(int mx, int my, int cx, int cy, int r) {
    return (mx - cx) * (mx - cx) + (my - cy) * (my - cy) <= r * r;
}

bool IsInRect(int mx, int my, int rx1, int ry1, int rx2, int ry2) {
    return mx >= rx1 && mx <= rx2 && my >= ry1 && my <= ry2;
}

void DrawButton(const TCHAR* label, int x1, int y1, int x2, int y2, bool active) {
    if (active) {
        setfillcolor(RGB(30, 41, 59)); 
        setlinecolor(RGB(34, 197, 94)); 
    }
    else {
        setfillcolor(RGB(241, 245, 249)); 
        setlinecolor(RGB(203, 213, 225));
    }
    fillroundrect(x1, y1, x2, y2, 8, 8);
    settextcolor(active ? WHITE : RGB(71, 85, 105));
    settextstyle(18, 0, _T("微软雅黑"), 0, 0, FW_BOLD, false, false, false);
    setbkmode(TRANSPARENT);
    int w = textwidth(label);
    int h = textheight(label);
    outtextxy(x1 + (x2 - x1 - w) / 2, y1 + (y2 - y1 - h) / 2, label);
}

void DrawArrow(int x1, int y1, int x2, int y2, COLORREF color) {
    setlinecolor(color);
    setlinestyle(PS_SOLID, 3);
    line(x1, y1, x2, y2);

    double angle = atan2(y2 - y1, x2 - x1);
    int r = 12;
    int ax1 = x2 - (int)(r * cos(angle - 0.4));
    int ay1 = y2 - (int)(r * sin(angle - 0.4));
    int ax2 = x2 - (int)(r * cos(angle + 0.4));
    int ay2 = y2 - (int)(r * sin(angle + 0.4));
    line(x2, y2, ax1, ay1);
    line(x2, y2, ax2, ay2);
}

// ----------------- 渲染函数（优化布局） -----------------
void Render(int start_node, int end_node, const vector<int>& vias, int vehicle, int decision, int hour, int minute, const GlobalResult& route_res) {
    // 1. 清空背景（地图区+侧边栏）
    setfillcolor(RGB(248, 250, 252));
    solidrectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // 2. 绘制地图区网格（仅0-MAP_AREA_MAX范围）
    setlinecolor(RGB(235, 240, 245));
    setlinestyle(PS_SOLID, 1);
    for (int i = 0; i < MAP_AREA_MAX; i += 30) line(i, 0, i, SCREEN_HEIGHT);
    for (int i = 0; i < SCREEN_HEIGHT; i += 30) line(0, i, MAP_AREA_MAX, i);

    // 3. 绘制侧边栏背景（区分地图区）
    setfillcolor(WHITE);
    setlinecolor(RGB(226, 232, 240));
    fillrectangle(MAP_AREA_MAX, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    // 绘制分隔线
    setlinecolor(RGB(203, 213, 225));
    line(MAP_AREA_MAX, 0, MAP_AREA_MAX, SCREEN_HEIGHT);

    // 4. 绘制所有城市间的连接线（仅地图区）
    for (int i = 0; i < G.vexnum; i++) {
        ArcNode* arc = G.vertices[i].firstarc;
        while (arc != NULL) {
            setlinecolor(RGB(210, 215, 225));
            setlinestyle(arc->type == TYPE_FLIGHT ? PS_DASH : PS_SOLID, 1);
            line(G.vertices[i].x, G.vertices[i].y, G.vertices[arc->adjvex].x, G.vertices[arc->adjvex].y);
            arc = arc->nextarc;
        }
    }

    // 5. 绘制最优路径箭头
    if (route_res.total_duration != INF && route_res.full_path.size() > 1) {
        for (size_t i = 0; i < route_res.full_path.size() - 1; i++) {
            int u = route_res.full_path[i];
            int v = route_res.full_path[i + 1];
            DrawArrow(G.vertices[u].x, G.vertices[u].y, G.vertices[v].x, G.vertices[v].y, RGB(34, 197, 94)); 
        }
    }

    // 6. 绘制城市节点（仅地图区）
    for (int i = 0; i < G.vexnum; i++) {
        bool is_start = (i == start_node);
        bool is_end = (i == end_node);
        bool is_via = (find(vias.begin(), vias.end(), i) != vias.end());

        COLORREF circle_color = WHITE;
        COLORREF border_color = RGB(148, 163, 184);
        if (is_start) {
            circle_color = RGB(34, 197, 94);  
            border_color = RGB(220, 252, 231);
        }
        else if (is_end) {
            circle_color = RGB(244, 63, 94);  
            border_color = RGB(254, 226, 226);
        }
        else if (is_via) {
            circle_color = RGB(245, 158, 11); 
            border_color = RGB(254, 243, 199);
        }

        setlinecolor(border_color);
        setlinestyle(PS_SOLID, 3);
        circle(G.vertices[i].x, G.vertices[i].y, 14);

        setfillcolor(circle_color);
        setlinecolor(RGB(71, 85, 105));
        setlinestyle(PS_SOLID, 1);
        fillcircle(G.vertices[i].x, G.vertices[i].y, 10);

        setbkmode(TRANSPARENT);
        settextcolor(RGB(15, 23, 42));
        settextstyle(18, 0, _T("微软雅黑"), 0, 0, FW_BOLD, false, false, false);
        outtextxy(G.vertices[i].x - textwidth(G.vertices[i].name) / 2, G.vertices[i].y + 18, G.vertices[i].name);
    }

    // ----------------- 侧边栏UI（固定坐标，分区排布） -----------------
    int sidebar_x = MAP_AREA_MAX + 20; // 侧边栏起始X坐标（560+20=580）
    int y = 20; // 侧边栏起始Y坐标

    // 标题
    settextcolor(RGB(15, 23, 42));
    settextstyle(20, 0, _T("微软雅黑"), 0, 0, FW_BOLD, false, false, false);
    outtextxy(sidebar_x, y, _T("交通咨询控制台"));
    y += 35;

    // 基础信息区
    settextstyle(18, 0, _T("微软雅黑"), 0, 0, FW_MEDIUM, false, false, false);
    settextcolor(RGB(51, 65, 85));
    
    TCHAR start_str[50] = _T("出发城市: [未选择]");
    if (start_node != -1) _stprintf_s(start_str, 50, _T("出发城市: %s (%s)"), G.vertices[start_node].name, G.vertices[start_node].code);
    outtextxy(sidebar_x, y, start_str);
    y += 28;

    TCHAR end_str[50] = _T("目的地:   [未选择]");
    if (end_node != -1) _stprintf_s(end_str, 50, _T("目的地:   %s (%s)"), G.vertices[end_node].name, G.vertices[end_node].code);
    outtextxy(sidebar_x, y, end_str);
    y += 28;

    TCHAR vias_str[50];
    _stprintf_s(vias_str, 50, _T("中转途径点: %d 个"), (int)vias.size());
    outtextxy(sidebar_x, y, vias_str);
    y += 30;

    // 交通工具选择区（两行两列，无重叠）
    DrawButton(_T("✈️ 飞机航空"), sidebar_x, y, sidebar_x+90, y+30, vehicle == TYPE_FLIGHT);
    DrawButton(_T("🚊 铁路干线"), sidebar_x+100, y, sidebar_x+190, y+30, vehicle == TYPE_TRAIN);
    y += 40;

    // 决策类型选择区
    DrawButton(_T("⚡ 最快到达"), sidebar_x, y, sidebar_x+90, y+30, decision == 1);
    DrawButton(_T("🪙 最省钱到达"), sidebar_x+100, y, sidebar_x+190, y+30, decision == 2);
    y += 40;

    // 出发时间调整区
    TCHAR time_btn_str[30];
    _stprintf_s(time_btn_str, 30, _T("出发时间: %02d:%02d"), hour, minute);
    DrawButton(time_btn_str, sidebar_x, y, sidebar_x+130, y+32, false);
    DrawButton(_T("➕"), sidebar_x+140, y, sidebar_x+165, y+32, false);
    DrawButton(_T("➖"), sidebar_x+175, y, sidebar_x+190, y+32, false);
    y += 40;

    // 功能按钮区（垂直排布，间距10）
    DrawButton(_T("🔀 随机生成途径点"), sidebar_x, y, sidebar_x+190, y+30, false);
    y += 35;
    DrawButton(_T("🔄 重置当前规划"), sidebar_x, y, sidebar_x+190, y+30, false);
    y += 35;
    DrawButton(_T("⚙️ 管理节点与联动"), sidebar_x, y, sidebar_x+190, y+30, false);
    y += 35;
    DrawButton(_T("🎯 开始最优检索"), sidebar_x, y, sidebar_x+190, y+30, true);
    y += 35;
    DrawButton(_T("🗑️ 删除选中节点"), sidebar_x, y, sidebar_x+190, y+30, false);
    y += 35;
    DrawButton(_T("🖥️ 切换分辨率"), sidebar_x, y, sidebar_x+190, y+30, false);
    y += 35; // 往下移动一点

    // 分割线
    setlinecolor(RGB(226, 232, 240));
    line(MAP_AREA_MAX + 10, y, 780, y);
    y += 15;

    // 结果展示区
    settextcolor(RGB(15, 23, 42));
    settextstyle(20, 0, _T("微软雅黑"), 0, 0, FW_BOLD, false, false, false);
    outtextxy(sidebar_x, y, _T("【智能乘车调度方案】")); 
    y += 25;

    settextstyle(18, 0, _T("微软雅黑"), 0, 0, FW_NORMAL, false, false, false);
    settextcolor(RGB(71, 85, 105));
    if (route_res.total_duration == INF) {
        outtextxy(sidebar_x, y, _T("无活动方案。请依次在地图上"));
        outtextxy(sidebar_x, y+20, _T("点击城市并点击“开始最优检索”。"));
    }
    else {
        TCHAR duration_str[100];
        _stprintf_s(duration_str, 100, _T("全程总耗时: %d小时%d分钟"), route_res.total_duration / 60, route_res.total_duration % 60);
        outtextxy(sidebar_x, y, duration_str);
        y += 20;

        TCHAR cost_str[100];
        _stprintf_s(cost_str, 100, _T("总旅行旅资: %.2f 元"), route_res.total_cost);
        outtextxy(sidebar_x, y, cost_str);
        y += 20;

        outtextxy(sidebar_x, y, _T("详细依次搭乘："));
        y += 15;
        for (size_t i = 0; i < route_res.full_schs.size() && i < 4; i++) {
            TCHAR step_info[150];
            TCHAR dep_time_str[30];
            TCHAR arr_time_str[30];
            MinToTimeStr(route_res.dep_times_absolute[i], dep_time_str, 30);
            MinToTimeStr(route_res.arr_times_absolute[i], arr_time_str, 30);

            int u_idx = route_res.full_path[i];
            int v_idx = route_res.full_path[i + 1];

            _stprintf_s(step_info, 150, _T("%s->%s: %s (%s开)"), G.vertices[u_idx].name, G.vertices[v_idx].name, route_res.full_schs[i]->no, dep_time_str);
            outtextxy(sidebar_x, y, step_info);
            y += 15;
        }
        if (route_res.full_schs.size() > 4) {
            outtextxy(sidebar_x, y, _T("...(省略后续中转步骤)..."));
        }
    }
}

// ----------------- 主函数 -----------------
int main() {
    InjectTestData();

    initgraph(800, 600);
    setbkmode(TRANSPARENT); // 设置背景透明，防止文字周围出现黑色底色
    BeginBatchDraw();

    int start_node = -1;
    int end_node = -1;
    vector<int> vias;
    int vehicle = TYPE_FLIGHT;
    int decision = 1; 
    int start_hour = 11;
    int start_minute = 0;

    GlobalResult route_result;
    route_result.total_duration = INF;

    ExMessage msg;
    while (true) {
        Render(start_node, end_node, vias, vehicle, decision, start_hour, start_minute, route_result);
        FlushBatchDraw();

        if (peekmessage(&msg, EM_MOUSE)) {
            if (msg.message == WM_LBUTTONDOWN) {
                int mx = msg.x;
                int my = msg.y;

                // A. 地图区城市节点点击（仅0-MAP_AREA_MAX范围）
                int clicked_node = -1;
                if (mx < MAP_AREA_MAX) {
                    for (int i = 0; i < G.vexnum; i++) {
                        if (IsInCircle(mx, my, G.vertices[i].x, G.vertices[i].y, 14)) {
                            clicked_node = i;
                            break;
                        }
                    }
                }

                if (clicked_node != -1) {
                    if (start_node == -1) {
                        start_node = clicked_node;
                    }
                    else if (end_node == -1) {
                        if (clicked_node != start_node) {
                            end_node = clicked_node;
                        }
                        else {
                            start_node = -1; 
                        }
                    }
                    else {
                        if (clicked_node == start_node) {
                            start_node = -1;
                        }
                        else if (clicked_node == end_node) {
                            end_node = -1;
                        }
                        else {
                            auto it = find(vias.begin(), vias.end(), clicked_node);
                            if (it != vias.end()) {
                                vias.erase(it); 
                            }
                            else {
                                vias.push_back(clicked_node); 
                            }
                        }
                    }
                    route_result.total_duration = INF; 
                }

                // B. 侧边栏按钮点击（MAP_AREA_MAX到800范围）
                int sb_x = MAP_AREA_MAX + 20;
                if (mx >= MAP_AREA_MAX) {
                    // 交通工具按钮
                    if (IsInRect(mx, my, sb_x, 140, sb_x+90, 170)) {
                        vehicle = TYPE_FLIGHT;
                        route_result.total_duration = INF;
                    }
                    if (IsInRect(mx, my, sb_x+100, 140, sb_x+190, 170)) {
                        vehicle = TYPE_TRAIN;
                        route_result.total_duration = INF;
                    }

                    // 决策类型按钮
                    if (IsInRect(mx, my, sb_x, 180, sb_x+90, 210)) {
                        decision = 1;
                        route_result.total_duration = INF;
                    }
                    if (IsInRect(mx, my, sb_x+100, 180, sb_x+190, 210)) {
                        decision = 2;
                        route_result.total_duration = INF;
                    }

                    // 时间调整按钮
                    if (IsInRect(mx, my, sb_x+140, 220, sb_x+165, 250)) {
                        start_hour = (start_hour + 1) % 24;
                        route_result.total_duration = INF;
                    }
                    if (IsInRect(mx, my, sb_x+175, 220, sb_x+190, 250)) {
                        start_hour = (start_hour + 23) % 24;
                        route_result.total_duration = INF;
                    }

                    // 功能按钮
                    if (IsInRect(mx, my, sb_x, 260, sb_x+190, 290)) {
                        if (start_node != -1 && end_node != -1) {
                            vias.clear();
                            vector<int> candidates;
                            for (int i = 0; i < G.vexnum; i++) {
                                if (i != start_node && i != end_node) candidates.push_back(i);
                            }
                            if (candidates.size() > 0) {
                                int count = rand() % 2 + 1;
                                std::random_device rd;
                                std::mt19937 g(rd());
                                std::shuffle(candidates.begin(), candidates.end(), g);
                                for (int i = 0; i < count && i < (int)candidates.size(); i++) {
                                    vias.push_back(candidates[i]);
                                }
                            }
                            route_result.total_duration = INF;
                        }
                    }
                    if (IsInRect(mx, my, sb_x, 295, sb_x+190, 325)) {
                        start_node = -1;
                        end_node = -1;
                        vias.clear();
                        route_result.total_duration = INF;
                    }
                    if (IsInRect(mx, my, sb_x, 330, sb_x+190, 360)) {
                        InteractiveEditNetwork();
                        route_result.total_duration = INF;
                    }
                    if (IsInRect(mx, my, sb_x, 365, sb_x+190, 395)) {
                        if (start_node != -1 && end_node != -1) {
                            int base_start_min = start_hour * 60 + start_minute;
                            route_result = SolveWithVias(start_node, end_node, base_start_min, decision, vehicle, vias);
                        }
                    }
                    //删除按钮
                
                    if (IsInRect(mx, my, sb_x, 400, sb_x+190, 430)) {
                        TCHAR input_name[30] = {0};
                        if (InputBox(input_name, 30, _T("请输入要删除的城市名称："), _T("删除节点"))) {
                            DeleteVertexByName(input_name);
                            // 删除后清空当前选择状态，防止索引错位引起崩溃
                            start_node = -1; end_node = -1; vias.clear();
                            route_result.total_duration = INF;
                        }
                    }
                    // 分辨率切换按钮 (坐标假设在 y=435 左右)
                    if (IsInRect(mx, my, sb_x, 435, sb_x+190, 465)) {
                        if (SCREEN_WIDTH == 800) { SCREEN_WIDTH = 1280; SCREEN_HEIGHT = 720; }
                        else if (SCREEN_WIDTH == 1280) { SCREEN_WIDTH = 1024; SCREEN_HEIGHT = 600; }
                        else { SCREEN_WIDTH = 800; SCREEN_HEIGHT = 600; }
                        MAP_AREA_MAX = SCREEN_WIDTH - SIDEBAR_WIDTH;
                        // 重新初始化画布
                        EndBatchDraw();
                        closegraph();
                        initgraph(SCREEN_WIDTH, SCREEN_HEIGHT);
                        // 这里需要更新你的 MAP_AREA_MAX 等常量，建议将它们改为变量而不是 const
                        BeginBatchDraw();
    
                        // 强制刷新
                        cleardevice();
                        continue; // 直接进入下一次循环渲染
                    
                    }
                }
            }
        }
        Sleep(16);
    }

    EndBatchDraw();
    closegraph();
    return 0;
}