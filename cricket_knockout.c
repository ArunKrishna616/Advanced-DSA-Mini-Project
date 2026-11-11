#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_TEAMS 64
#define LEN 32

typedef struct {
    char name[LEN];
    char captain[LEN];
    char city[LEN];
} Team;

typedef struct Node {
    Team *team;
    struct Node *left;
    struct Node *right;
    int match_no;
    int height;
} Node;

static Team teams[MAX_TEAMS];
static int nteams = 0;

static Node *root = NULL;
static Node *matches[MAX_TEAMS];
static int nmatches = 0;

static int read_line(char *buf, int size)
{
    if (!fgets(buf, size, stdin)) {
        return 0;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

static void quit_on_eof(void)
{
    printf("\nGoodbye!\n");
    exit(0);
}

static void read_text(const char *prompt, char *out, const char *fallback)
{
    char buf[128];
    char *s;
    size_t n;

    printf("%s", prompt);
    if (!read_line(buf, sizeof buf)) {
        quit_on_eof();
    }

    s = buf;
    while (isspace((unsigned char)*s)) {
        s++;
    }
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
    if (n == 0) {
        s = (char *)fallback;
    }

    strncpy(out, s, LEN - 1);
    out[LEN - 1] = '\0';
}

static int read_int(const char *prompt, int lo, int hi)
{
    char buf[64];
    char *end;
    long v;

    for (;;) {
        printf("%s", prompt);
        if (!read_line(buf, sizeof buf)) {
            quit_on_eof();
        }
        v = strtol(buf, &end, 10);
        if (end != buf && *end == '\0' && v >= lo && v <= hi) {
            return (int)v;
        }
        printf("  Invalid input, try again.\n");
    }
}

static int same_name(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static Node *new_node(Team *t, Node *l, Node *r)
{
    Node *n = calloc(1, sizeof *n);

    if (!n) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    n->team = t;
    n->left = l;
    n->right = r;
    if (l) {
        n->height = 1 + (l->height > r->height ? l->height : r->height);
    }
    return n;
}

static Node *build_tree(int lo, int hi)
{
    int mid;

    if (hi - lo == 1) {
        return new_node(&teams[lo], NULL, NULL);
    }
    mid = lo + (hi - lo) / 2;
    return new_node(NULL, build_tree(lo, mid), build_tree(mid, hi));
}

static void free_tree(Node *n)
{
    if (!n) {
        return;
    }
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

static void inorder(Node *n, Node **out, int *count)
{
    if (!n || !n->left) {
        return;
    }
    inorder(n->left, out, count);
    out[(*count)++] = n;
    inorder(n->right, out, count);
}

static void round_name(int round, int total, char *out)
{
    switch (total - round) {
    case 0:
        strcpy(out, "Final");
        break;
    case 1:
        strcpy(out, "Semi-Final");
        break;
    case 2:
        strcpy(out, "Quarter-Final");
        break;
    default:
        sprintf(out, "Round %d", round);
        break;
    }
}

static void label(const Node *c, char *out)
{
    if (c->team) {
        strcpy(out, c->team->name);
    } else {
        sprintf(out, "Winner of M%d", c->match_no);
    }
}

static void register_teams(void)
{
    int n, i, j, dup;
    char def[LEN];

    if (root) {
        printf("Fixtures already generated. Reset to register again.\n");
        return;
    }

    n = read_int("Number of teams (power of 2: 2, 4, 8, 16, 32, 64): ", 2, MAX_TEAMS);
    while (n & (n - 1)) {
        printf("  Must be a power of two.\n");
        n = read_int("Number of teams: ", 2, MAX_TEAMS);
    }

    nteams = 0;
    for (i = 0; i < n; i++) {
        sprintf(def, "Team %d", i + 1);
        printf("Team %d\n", i + 1);

        for (;;) {
            read_text("  Name   : ", teams[i].name, def);
            dup = 0;
            for (j = 0; j < i; j++) {
                if (same_name(teams[j].name, teams[i].name)) {
                    dup = 1;
                }
            }
            if (!dup) {
                break;
            }
            printf("  Name already used.\n");
        }

        read_text("  Captain: ", teams[i].captain, "-");
        read_text("  City   : ", teams[i].city, "-");
        nteams = i + 1;
    }

    printf("%d teams registered.\n", n);
}

static void show_teams(void)
{
    int i;

    if (nteams == 0) {
        printf("No teams registered yet.\n");
        return;
    }

    printf("\n%-4s%-20s%-18s%s\n", "#", "Team", "Captain", "City");
    for (i = 0; i < nteams; i++) {
        printf("%-4d%-20s%-18s%s\n", i + 1, teams[i].name, teams[i].captain, teams[i].city);
    }
}

static void show_fixtures(void)
{
    int r, i;
    char rn[32], a[LEN + 16], b[LEN + 16];
    Node *m;

    if (!root) {
        printf("Generate fixtures first.\n");
        return;
    }

    for (r = 1; r <= root->height; r++) {
        round_name(r, root->height, rn);
        printf("\n--- %s ---\n", rn);

        for (i = 0; i < nmatches; i++) {
            m = matches[i];
            if (m->height != r) {
                continue;
            }

            label(m->left, a);
            label(m->right, b);
            printf("  M%d: %s vs %s\n", m->match_no, a, b);
        }
    }
}

static void generate_fixtures(void)
{
    char buf[16];
    Node *all[MAX_TEAMS];
    int count = 0;
    int h, i, j;
    Team tmp;

    if (nteams == 0) {
        printf("Register teams first.\n");
        return;
    }
    if (root) {
        printf("Fixtures already generated.\n");
        return;
    }

    printf("Random draw? (y/n): ");
    if (!read_line(buf, sizeof buf)) {
        quit_on_eof();
    }
    if (tolower((unsigned char)buf[0]) == 'y') {
        for (i = nteams - 1; i > 0; i--) {
            j = rand() % (i + 1);
            tmp = teams[i];
            teams[i] = teams[j];
            teams[j] = tmp;
        }
    }

    root = build_tree(0, nteams);

    inorder(root, all, &count);
    nmatches = 0;
    for (h = 1; h <= root->height; h++) {
        for (i = 0; i < count; i++) {
            if (all[i]->height == h) {
                all[i]->match_no = nmatches + 1;
                matches[nmatches++] = all[i];
            }
        }
    }

    printf("Fixtures generated: %d teams, %d matches (N-1), %d rounds.\n",
           nteams, nmatches, root->height);
    show_fixtures();
}

static void draw_bracket(const Node *n, int depth)
{
    if (!n) {
        return;
    }

    draw_bracket(n->right, depth + 1);

    printf("%*s", depth * 6, "");
    if (!n->left) {
        printf("%s\n", n->team->name);
    } else {
        printf("M%d\n", n->match_no);
    }

    draw_bracket(n->left, depth + 1);
}

static void show_bracket(void)
{
    if (!root) {
        printf("Generate fixtures first.\n");
        return;
    }

    printf("\nBracket (root = Final, read sideways):\n\n");
    draw_bracket(root, 0);
}

static void reset(void)
{
    free_tree(root);
    root = NULL;
    nteams = 0;
    nmatches = 0;
    printf("Tournament reset.\n");
}

int main(void)
{
    int choice;

    srand((unsigned)time(NULL));

    for (;;) {
        printf("\n===== Cricket Knockout Tournament Fixture Generator =====\n");
        printf(" 1. Register teams\n");
        printf(" 2. View teams\n");
        printf(" 3. Generate fixtures\n");
        printf(" 4. View round-wise fixtures\n");
        printf(" 5. View bracket (tree)\n");
        printf(" 6. Reset\n");
        printf(" 0. Exit\n");

        choice = read_int("Choice: ", 0, 6);

        switch (choice) {
        case 1:
            register_teams();
            break;
        case 2:
            show_teams();
            break;
        case 3:
            generate_fixtures();
            break;
        case 4:
            show_fixtures();
            break;
        case 5:
            show_bracket();
            break;
        case 6:
            reset();
            break;
        case 0:
            free_tree(root);
            printf("Goodbye!\n");
            return 0;
        }
    }
}
