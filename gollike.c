/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright (c) 2026 Evdokimov Stepan                                       *
 *                                                                           *
 * Permission is hereby granted, free of charge, to any person obtaining a   *
 * copy of this software and associated documentation files (the "Software"),*
 * to deal in the Software without restriction, including without limitation *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,  *
 * and/or sell copies of the Software, and to permit persons to whom the     *
 * Software is furnished to do so, subject to the following conditions:      *
 *                                                                           *
 * The above copyright notice and this permission notice shall be included   *
 * in all copies or substantial portions of the Software.                    *
 *                                                                           *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS   *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN *
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,  *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR     *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                    *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if defined(__linux__)
#  define _DEFAULT_SOURCE
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#if __STDC_VERSION__ >= 199901L
#  include <stdbool.h>
#else
typedef unsigned char bool;
#  define false 0
#  define true  1
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                              Global constants                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DEAD_CELL " "
#if USE_ASCII_GRAPHIC
#  define ALIVE_CELL   "#"
#  define  DEAD_CURSOR "."
#  define ALIVE_CURSOR "%"
#  define LU_CORNER    "+"
#  define RU_CORNER    "+"
#  define LD_CORNER    "+"
#  define RD_CORNER    "+"
#  define  HOR_BAR     "-"
#  define  VER_BAR     "|"
#  define LVER_BAR     "<"
#  define RVER_BAR     ">"
#else
#  define ALIVE_CELL   "\xe2\x96\x88" /* U+2588 */
#  define  DEAD_CURSOR "\xe2\x96\x91" /* U+2591 */
#  define ALIVE_CURSOR "\xe2\x96\x93" /* U+2593 */
#  define LU_CORNER    "\xe2\x95\x94" /* U+2554 */
#  define RU_CORNER    "\xe2\x95\x97" /* U+2557 */
#  define LD_CORNER    "\xe2\x95\x9a" /* U+255A */
#  define RD_CORNER    "\xe2\x95\x9d" /* U+255D */
#  define  HOR_BAR     "\xe2\x95\x90" /* U+2550 */
#  define  VER_BAR     "\xe2\x95\x91" /* U+2551 */
#  define LVER_BAR     "\xe2\x95\xa1" /* U+2561 */
#  define RVER_BAR     "\xe2\x95\x9e" /* U+255E */
#endif

#define    DEAD_CELL_BLOCK DEAD_CELL DEAD_CELL
#define   ALIVE_CELL_BLOCK ALIVE_CELL ALIVE_CELL
#define  DEAD_CURSOR_BLOCK DEAD_CURSOR DEAD_CURSOR
#define ALIVE_CURSOR_BLOCK ALIVE_CURSOR ALIVE_CURSOR

#define MIN_FIELD_WIDTH  25
#define MIN_FIELD_HEIGHT 1
#define MAX_FIELD_WIDTH  1000
#define MAX_FIELD_HEIGHT 1000

#define DEFAULT_RULE   "B3/S23/G2"
#define DEFAULT_PROB   0.5
#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 25
#define DEFAULT_INDENT 0

#define INVALID_BS_MASK (-1ul)

#define FRAMES_REP_SECOND    50 /* frame ~ one simulation step */
#define DELAY_IN_MILLISECOND (1000 / FRAMES_REP_SECOND)

#define MAX_RULE_LENGTH     26
#define COUNT_TEMPLATE_SLOT 10

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                          Support macro-functions                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define Stringify(x) # x
#define stringify(x) Stringify(x)

#define Concat(x, y) x ## y
#define concat(x, y) Concat(x, y)

#define static_assert(cond) typedef struct { \
    char a[(cond) ? 1 : -1]; \
} concat(__static_assert_, __LINE__)

#define error_msg(message) do { \
    fprintf(stderr, "ERROR: "message"\n"); \
    goto error; \
} while (0)

#define error_msgf(fmt, arg) do { \
    fprintf(stderr, "ERROR: "fmt"\n", arg); \
    goto error; \
} while (0)

#define strlitlen(literal) (sizeof(literal) - 1)
#define   shift_arg() (--argc, *argv++)
#define unshift_arg() (++argc, --argv)
#define min(a, b)     ((a) < (b) ? (a) : (b))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                Help message                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define HELPMSG_NAME \
    "NAME:"                                                 "\n" \
    "    gollike - simulator of Game of Life-like automata" "\n" \

#define HELPMSG_USAGE \
    "USAGE:"                "\n" \
    "  $ gollike [OPTIONS]" "\n" \

#define HELPMSG_OPTIONS_PT1 \
    "OPTIONS:"                                                                                                   "\n" \
    "    -H, --help                    Outputs this message and terminates the program"                          "\n" \
    "    -r, --rule <string>           Sets a rule for a cellular automaton, using the format described below"   "\n" \
    "    -c, --colors <string>         Sets palette for drawing cell states, using format described below"       "\n" \
    "    -p, --probability <number>    Sets the probability of a cell appearing at the beginning and at restart" "\n" \
    "    -1, -2, ..., -9 <string>      Sets a template in slot #, using format described below"                  "\n" \

#define HELPMSG_OPTIONS_PT2 \
    "    -a, --autofit                 Sets width and height of field from size of console"     "\n" \
    "    -w, --width  <integer>        Sets width of field"                                     "\n" \
    "    -h, --height <integer>        Sets height of field"                                    "\n" \
    "    -i, --indent <integer>        Sets indent from border for spawning cells"              "\n" \

#define HELPMSG_KEYS_COMMON \
    "CONTROL KEYS:"                               "\n" \
    "  All mode:"                                 "\n" \
    "    Q - Quit from program"                   "\n" \
    "    E - Switch to edit/simulation mode"      "\n" \
    "    W - Move the camera/cursor up"           "\n" \
    "    S - Move the camera/cursor down"         "\n" \
    "    A - Move the camera/cursor to the left"  "\n" \
    "    D - Move the camera/cursor to the right" "\n" \

#define HELPMSG_KEYS_SIM \
    "  Simulation mode:"                                  "\n" \
    "    R - Reset simulation"                            "\n" \
    "  S+R - Reset simulation with only full alive cells" "\n" \
    "    P - Set/unset pause"                             "\n" \
    "    O - Make one simulation step in pause"           "\n" \

#define HELPMSG_KEYS_EDIT \
    "  Edit mode:"                                        "\n" \
    "  S+W - Move the camera/cursor up 10 step"           "\n" \
    "  S+S - Move the camera/cursor down 10 step"         "\n" \
    "  S+A - Move the camera/cursor to the left 10 step"  "\n" \
    "  S+D - Move the camera/cursor to the right 10 step" "\n" \
    "    R - Enable/disable rectagular selection"         "\n" \
    "  S+C - Clear all field"                             "\n" \
    "    G - Make the cell dead"                          "\n" \
    "    B - Make the cell alive"                         "\n" \
    "    T - Toggle the cell state"                       "\n" \
    "    C - Copy selected area to buffer"                "\n" \
    "    X - Cut selected area to buffer"                 "\n" \
    "    0 - Enable template from buffer"                 "\n" \
    "  1-9 - Enable template with number #"               "\n" \

#define HELPMSG_KEYS_TEMPLATE \
    "  Template mode:"                                          "\n" \
    "    E - Exit from template mode"                           "\n" \
    "    P - Paste template and rewrite all cells in rect area" "\n" \
    "    O - Overlay template with write only alive cells"      "\n" \
    "    F - Flip template by horizontal"                       "\n" \
    "  S+F - Flip template by vertical"                         "\n" \
    "    G - 180 degree rotation of template"                   "\n" \

#define HELPMSG_RULE_SYNTAX \
    "RULE SYNTAX:"                                                 "\n" \
    "  Pattern (case insensitive): B<digits>/S<digits>[/G<count>]" "\n" \
    "    <digits> in the range from 0 to 8 inclusive"              "\n" \
    "    B<digits> - The number of neighbors to become alive"      "\n" \
    "    S<digits> - The number of neighbors to stay alive"        "\n" \
    "    G<count>  - The count of possible states (default 2)"     "\n" \

#define HELPMSG_RULE_EXAMPLE \
    "  Examples:"                                              "\n" \
    "    B3/S23        - Conway's Game of life (default rule)" "\n" \
    "    B3/S012345678 - Life without Death"                   "\n" \
    "    B3678/S34678  - Day & Night"                          "\n" \
    "    B35678/S5678  - Diamoeba"                             "\n" \
    "    B368/S245     - Morley"                               "\n" \
    "    B34/S34       - 34 Life"                              "\n" \
    "    B2/S          - Seeds"                                "\n" \
    "    B2/S/G3       - Brian's Brain"                        "\n" \
    "    B2/S345/G4    - Star Wars"                            "\n" \
    "    B34/S12/G3    - Frogs"                                "\n" \

#define HELPMSG_TEMPLATE_SYNTAX \
    "TEMPLATE SYNTAX:"                                                       "\n" \
    "  Regex-like: <width>:<height>:(<repeate>?<tag>)*!"                     "\n" \
    "                               \\  RLE of figure  /"                    "\n" \
    "    <width>   - Width of template"                                      "\n" \
    "    <height>  - Height of template"                                     "\n" \
    "    <repeate> - Number of repetitions <tag>, greater or equal than 1"   "\n" \
    "    <tag>     - 'b' is dead cell, 'o' is alive cell and '$' is newline" "\n" \
    "    allows the use of whitespace characters between tags in RLE"        "\n" \

#define HELPMSG_TEMPLATE_EXAMPLE_PT1 \
    "  Examples:"                "\n" \
    "    3:3:bo$2bo$3o!"         "\n" \
    "    |             .#."      "\n" \
    "    +-> glider -> ..#"      "\n" \
    "                  ###"      "\n" \
    ""                           "\n" \
    "    5:4:b4o$o3bo$4bo$o2bo!" "\n" \
    "    |           .####"      "\n" \
    "    +-> LWSS -> #...#"      "\n" \
    "                ....#"      "\n" \
    "                #..#."      "\n" \

#define HELPMSG_TEMPLATE_EXAMPLE_PT2 \
    "    36:9:"                                              "\n" \
    "    24bo11b$22bobo11b$12b2o6b2o12b2o$11bo3bo4b2o12b2o$" "\n" \
    "    2o8bo5bo3b2o14b$2o8bo3bob2o4bobo11b$10bo5bo7bo11b$" "\n" \
    "    11bo3bo20b$12b2o!"                                  "\n" \

#define HELPMSG_TEMPLATE_EXAMPLE_PT3 \
    "    |              ........................#..........." "\n" \
    "    |              ......................#.#..........." "\n" \
    "    |              ............##......##............##" "\n" \
    "    |   Gosper     ...........#...#....##............##" "\n" \
    "    +-> glider  -> ##........#.....#...##.............." "\n" \
    "        gun        ##........#...#.##....#.#..........." "\n" \
    "                   ..........#.....#.......#..........." "\n" \
    "                   ...........#...#...................." "\n" \
    "                   ............##......................" "\n" \

#define STDCLR_N \
    ESC"48;5;0m 0" ESC"30m" ESC"48;5;1m 1" ESC"48;5;2m 2" ESC"48;5;3m 3" \
    ESC"48;5;4m 4"          ESC"48;5;5m 5" ESC"48;5;6m 6" ESC"48;5;7m 7"

#define STDCLR_B ESC"30m" \
    ESC "48;5;8m 8" ESC "48;5;9m 9" ESC"48;5;10m10" ESC"48;5;11m11" \
    ESC"48;5;12m12" ESC"48;5;13m13" ESC"48;5;14m14" ESC"48;5;15m15"

#define HELPMSG_COLORS_PT1 \
    "COLOR PALETTE:"                                                   "\n" \
    "  Parameter format:"                                              "\n" \
    "    <string> is list of color identificators separeted by spaces" "\n" \
    "" "\n" \
    "  Standard terminal colors:"          "\n" \
    "    " STDCLR_N ESC"0m"                "\n" \
    "  Standard terminal bright colors:"   "\n" \
    "    " STDCLR_B ESC"0m"                "\n" \
    "  RGB cude 6x6x6 (0 <= r, g, b <= 5)" "\n" \
    "    id = 16 + 36*r + 6*g + b"         "\n" \

#define COLORFACE_R0_WH \
    ESC"48;5;16m 16" ESC"48;5;17m 17" ESC"48;5;18m 18" ESC"48;5;19m 19" ESC"48;5;20m 20" ESC"48;5;21m 21" \
    ESC"48;5;22m 22" ESC"48;5;23m 23" ESC"48;5;24m 24" ESC"48;5;25m 25" ESC"48;5;26m 26" ESC"48;5;27m 27" \
    ESC"48;5;28m 28" ESC"48;5;29m 29" ESC"48;5;30m 30" ESC"48;5;31m 31" ESC"48;5;32m 32" ESC"48;5;33m 33"
#define COLORFACE_R0_BL ESC"30m" \
    ESC"48;5;34m 34" ESC"48;5;35m 35" ESC"48;5;36m 36" ESC"48;5;37m 37" ESC"48;5;38m 38" ESC"48;5;39m 39" \
    ESC"48;5;40m 40" ESC"48;5;41m 41" ESC"48;5;42m 42" ESC"48;5;43m 43" ESC"48;5;44m 44" ESC"48;5;45m 45" \
    ESC"48;5;46m 46" ESC"48;5;47m 47" ESC"48;5;48m 48" ESC"48;5;49m 49" ESC"48;5;50m 50" ESC"48;5;51m 51"

#define COLORFACE_R1_WH \
    ESC"48;5;52m 52" ESC"48;5;53m 53" ESC"48;5;54m 54" ESC"48;5;55m 55" ESC"48;5;56m 56" ESC"48;5;57m 57" \
    ESC"48;5;58m 58" ESC"48;5;59m 59" ESC"48;5;60m 60" ESC"48;5;61m 61" ESC"48;5;62m 62" ESC"48;5;63m 63" \
    ESC"48;5;64m 64" ESC"48;5;65m 65" ESC"48;5;66m 66" ESC"48;5;67m 67" ESC"48;5;68m 68" ESC"48;5;69m 69"
#define COLORFACE_R1_BL ESC"30m" \
    ESC"48;5;70m 70" ESC"48;5;71m 71" ESC"48;5;72m 72" ESC"48;5;73m 73" ESC"48;5;74m 74" ESC"48;5;75m 75" \
    ESC"48;5;76m 76" ESC"48;5;77m 77" ESC"48;5;78m 78" ESC"48;5;79m 79" ESC"48;5;80m 80" ESC"48;5;81m 81" \
    ESC"48;5;82m 82" ESC"48;5;83m 83" ESC"48;5;84m 84" ESC"48;5;85m 85" ESC"48;5;86m 86" ESC"48;5;87m 87"

#define COLORFACE_R2_WH \
    ESC "48;5;88m 88" ESC "48;5;89m 89" ESC "48;5;90m 90" ESC "48;5;91m 91" ESC "48;5;92m 92" ESC "48;5;93m 93" \
    ESC "48;5;94m 94" ESC "48;5;95m 95" ESC "48;5;96m 96" ESC "48;5;97m 97" ESC "48;5;98m 98" ESC "48;5;99m 99" \
    ESC"48;5;100m100" ESC"48;5;101m101" ESC"48;5;102m102" ESC"48;5;103m103" ESC"48;5;104m104" ESC"48;5;105m105"
#define COLORFACE_R2_BL ESC"30m" \
    ESC"48;5;106m106" ESC"48;5;107m107" ESC"48;5;108m108" ESC"48;5;109m109" ESC"48;5;110m110" ESC"48;5;111m111" \
    ESC"48;5;112m112" ESC"48;5;113m113" ESC"48;5;114m114" ESC"48;5;115m115" ESC"48;5;116m116" ESC"48;5;117m117" \
    ESC"48;5;118m118" ESC"48;5;119m119" ESC"48;5;120m120" ESC"48;5;121m121" ESC"48;5;122m122" ESC"48;5;123m123"

#define COLORFACE_R3_WH \
    ESC"48;5;124m124" ESC"48;5;125m125" ESC"48;5;126m126" ESC"48;5;127m127" ESC"48;5;128m128" ESC"48;5;129m129" \
    ESC"48;5;130m130" ESC"48;5;131m131" ESC"48;5;132m132" ESC"48;5;133m133" ESC"48;5;134m134" ESC"48;5;135m135" \
    ESC"48;5;136m136" ESC"48;5;137m137" ESC"48;5;138m138" ESC"48;5;139m139" ESC"48;5;140m140" ESC"48;5;141m141"
#define COLORFACE_R3_BL ESC"30m" \
    ESC"48;5;142m142" ESC"48;5;143m143" ESC"48;5;144m144" ESC"48;5;145m145" ESC"48;5;146m146" ESC"48;5;147m147" \
    ESC"48;5;148m148" ESC"48;5;149m149" ESC"48;5;150m150" ESC"48;5;151m151" ESC"48;5;152m152" ESC"48;5;153m153" \
    ESC"48;5;154m154" ESC"48;5;155m155" ESC"48;5;156m156" ESC"48;5;157m157" ESC"48;5;158m158" ESC"48;5;159m159"

#define COLORFACE_R4_WH \
    ESC"48;5;160m160" ESC"48;5;161m161" ESC"48;5;162m162" ESC"48;5;163m163" ESC"48;5;164m164" ESC"48;5;165m165" \
    ESC"48;5;166m166" ESC"48;5;167m167" ESC"48;5;168m168" ESC"48;5;169m169" ESC"48;5;170m170" ESC"48;5;171m171" \
    ESC"48;5;172m172" ESC"48;5;173m173" ESC"48;5;174m174" ESC"48;5;175m175" ESC"48;5;176m176" ESC"48;5;177m177"
#define COLORFACE_R4_BL ESC"30m" \
    ESC"48;5;178m178" ESC"48;5;179m179" ESC"48;5;180m180" ESC"48;5;181m181" ESC"48;5;182m182" ESC"48;5;183m183" \
    ESC"48;5;184m184" ESC"48;5;185m185" ESC"48;5;186m186" ESC"48;5;187m187" ESC"48;5;188m188" ESC"48;5;189m189" \
    ESC"48;5;190m190" ESC"48;5;191m191" ESC"48;5;192m192" ESC"48;5;193m193" ESC"48;5;194m194" ESC"48;5;195m195"

#define COLORFACE_R5_WH \
    ESC"48;5;196m196" ESC"48;5;197m197" ESC"48;5;198m198" ESC"48;5;199m199" ESC"48;5;200m200" ESC"48;5;201m201" \
    ESC"48;5;202m202" ESC"48;5;203m203" ESC"48;5;204m204" ESC"48;5;205m205" ESC"48;5;206m206" ESC"48;5;207m207" \
    ESC"48;5;208m208" ESC"48;5;209m209" ESC"48;5;210m210" ESC"48;5;211m211" ESC"48;5;212m212" ESC"48;5;213m213"
#define COLORFACE_R5_BL ESC"30m" \
    ESC"48;5;214m214" ESC"48;5;215m215" ESC"48;5;216m216" ESC"48;5;217m217" ESC"48;5;218m218" ESC"48;5;219m219" \
    ESC"48;5;220m220" ESC"48;5;221m221" ESC"48;5;222m222" ESC"48;5;223m223" ESC"48;5;224m224" ESC"48;5;225m225" \
    ESC"48;5;226m226" ESC"48;5;227m227" ESC"48;5;228m228" ESC"48;5;229m229" ESC"48;5;230m230" ESC"48;5;231m231"

#define HELPMSG_COLORS_PT2 \
    "  Black to white gradient" "\n" \
    "    "   ESC"48;5;232m232" ESC"48;5;233m233" ESC"48;5;234m234" ESC"48;5;235m235" ESC"48;5;236m236" ESC"48;5;237m237" \
             ESC"48;5;238m238" ESC"48;5;239m239" ESC"48;5;240m240" ESC"48;5;241m241" ESC"48;5;242m242" ESC"48;5;243m243" \
    ESC"30m" ESC"48;5;244m244" ESC"48;5;245m245" ESC"48;5;246m246" ESC"48;5;247m247" ESC"48;5;248m248" ESC"48;5;249m249" \
             ESC"48;5;250m250" ESC"48;5;251m251" ESC"48;5;252m252" ESC"48;5;253m253" ESC"48;5;254m254" ESC"48;5;255m255" \
    ESC"0m" "\n" \

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                        Information and mode string                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define HOR_BAR_LINE \
    HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR \
    HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR \
    HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR \
    HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR \
    HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR HOR_BAR \

/* Maximum width case  * * * * * * * * * * * * * * * * *
 * -< B012345678/S012345678/G256 | 1000x1000/0.999 >-  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define INFOFMT " %s " VER_BAR " %lux%lu/%.3f "

#define MODE_TXT_SIMULATION   "SIMULATION"
#define MODE_TXT_PAUSE        "PAUSE"
#define MODE_TXT_CURSOR       "CURSOR: %lu %lu"
#define MODE_TXT_RECTANGLE    "RECTANGLE: %lu %lu"
#define MODE_TXT_TEMPLATE     "TEMPLATE #%lu"
#define MODE_TXT_TEMPLATE_BUF "TEMPLATE BUFFER"

#define CLEAR_BAR fputs(HOR_BAR_LINE ESC"3G", stdout)
#define PUT_BAR_SIMULATION   do { CLEAR_BAR; fputs(LVER_BAR" "MODE_TXT_SIMULATION  " "RVER_BAR, stdout); } while (0)
#define PUT_BAR_PAUSE        do { CLEAR_BAR; fputs(LVER_BAR" "MODE_TXT_PAUSE       " "RVER_BAR, stdout); } while (0)
#define PUT_BAR_TEMPLATE_BUF do { CLEAR_BAR; fputs(LVER_BAR" "MODE_TXT_TEMPLATE_BUF" "RVER_BAR, stdout); } while (0)
#define PUT_BAR_CURSOR do { CLEAR_BAR; \
    printf(LVER_BAR" "MODE_TXT_CURSOR" "RVER_BAR, cursor_x, cursor_y); \
} while (0)
#define PUT_BAR_RECTANGLE do { CLEAR_BAR; \
    printf(LVER_BAR" "MODE_TXT_RECTANGLE" "RVER_BAR, \
        cursor_x - rect_x + 1, cursor_y - rect_y + 1); \
} while (0)
#define PUT_BAR_TEMPLATE do { CLEAR_BAR; \
    printf(LVER_BAR" "MODE_TXT_TEMPLATE" "RVER_BAR, index + 1); \
} while (0)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                Static asserts for checking constant values                *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static_assert(strlitlen(HOR_BAR_LINE) / strlitlen(HOR_BAR) == 2 * MIN_FIELD_WIDTH);

static_assert(strlitlen(MODE_TXT_SIMULATION  ) + 6 <= 2 * MIN_FIELD_WIDTH);
static_assert(strlitlen(MODE_TXT_PAUSE       ) + 6 <= 2 * MIN_FIELD_WIDTH);
static_assert(strlitlen(MODE_TXT_CURSOR      ) + 6 <= 2 * MIN_FIELD_WIDTH);
static_assert(strlitlen(MODE_TXT_RECTANGLE   ) + 6 <= 2 * MIN_FIELD_WIDTH);
static_assert(strlitlen(MODE_TXT_TEMPLATE    ) + 4 <= 2 * MIN_FIELD_WIDTH);
static_assert(strlitlen(MODE_TXT_TEMPLATE_BUF) + 6 <= 2 * MIN_FIELD_WIDTH);

static_assert(strlitlen(DEFAULT_RULE) <= MAX_RULE_LENGTH);
static_assert(MIN_FIELD_WIDTH  <= DEFAULT_WIDTH  && DEFAULT_WIDTH  <= MAX_FIELD_WIDTH );
static_assert(MIN_FIELD_HEIGHT <= DEFAULT_HEIGHT && DEFAULT_HEIGHT <= MAX_FIELD_HEIGHT);
static_assert(DEFAULT_INDENT <= DEFAULT_WIDTH  / 2);
static_assert(DEFAULT_INDENT <= DEFAULT_HEIGHT / 2);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                         Main and support functions                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Rule bit interpretation * * * * * *
 * Example rule B368/S245 (Morley)   *
 *    bit index :        1         0 *
 *                765432109876543210 *
 *   birth bits :          101001000 *
 * survive bits : 000110100          *
 * bsmask (hex) =               6948 *
 * * * * * * * * * * * * * * * * * * */

#define ESC "\x1b["

typedef unsigned char uchar;
typedef unsigned long ulong;

typedef struct {
    uchar* array;
    ulong  width;
    ulong height;
} template_t;

typedef enum {
    MODE_SIMULATION = 0,
    MODE_PAUSE,
    MODE_ONESTEP,

    MODE_CURSOR,
    MODE_RECTANGLE,

    MODE_TEMPLATE_1,
    MODE_TEMPLATE_2,
    MODE_TEMPLATE_3,
    MODE_TEMPLATE_4,
    MODE_TEMPLATE_5,
    MODE_TEMPLATE_6,
    MODE_TEMPLATE_7,
    MODE_TEMPLATE_8,
    MODE_TEMPLATE_9,
    MODE_TEMPLATE_BUF
} mode_action_t;

static char state_colors[256][16] = {0};

float randf0t1(void);
uchar randrange(uchar max);

size_t prev_size_t(size_t value, ulong len);
size_t next_size_t(size_t value, ulong len);

ulong parse_rule(const char* str, uchar* gens);
template_t parse_rle(const char* rle, uchar gens, ulong width, ulong height);
void normalization_rule(char* rule, ulong mask, uchar gens);

void draw_border(ulong w, ulong h);

void move_to_up   (uchar* field, size_t width, size_t heigth);
void move_to_down (uchar* field, size_t width, size_t heigth);
void move_to_left (uchar* field, size_t width, size_t heigth);
void move_to_right(uchar* field, size_t width, size_t heigth);

void flip_horizontally(template_t* tmpl);
void flip_vertically  (template_t* tmpl);
void rotate_by_180deg (template_t* tmpl);

bool get_console_size(ulong* width, ulong* height);
void sleep_ms(ulong ms);
bool is_symbol_received(void);
int received_symbol(void);

#if defined(__linux__)
static struct termios orig_termios;
static struct termios  new_termios;
static int peek_char = -1;

void setup_terminal(void);
#else
static char output_buffer[8 * 1024];
#endif

int main(int argc, char** argv) {
    size_t i, j; int rc = EXIT_FAILURE;

    /* Parameters of simulation */
    ulong width, height, indent, bsmask;
    char rule[MAX_RULE_LENGTH + 1];
    float prob; uchar gens;

    bool full_alive_only = false;

    /* Coordinates */
    ulong cursor_x = 0, cursor_y = 0;
    ulong   rect_x = 0,   rect_y = 0;

    /* Program mode, aka state */
    mode_action_t mode = MODE_SIMULATION;

    /* Slots with pattern for paste */
    template_t template_slots[COUNT_TEMPLATE_SLOT] = {0};

    /* arrays with current and next field */
    uchar* field_fst = NULL;
    uchar* field_snd = NULL;
#define FSTF(i, j) field_fst[width * (i) + (j)]
#define SNDF(i, j) field_snd[width * (i) + (j)]

    /* Flag parsed options */
    bool   rule_is_set = false;
    bool   prob_is_set = false;
    bool  width_is_set = false;
    bool height_is_set = false;
    bool indent_is_set = false;
    bool colors_is_set = false;

    (void)shift_arg(); /* skip program name */

    /* Argument parsing loop */
    while (argc > 0) {
        char* opt = shift_arg();
        char* arg = shift_arg(); /* always argument or NULL, no UB */
        char* end;

        /*  */ if (strcmp(opt, "-H") == 0 || strcmp(opt, "--help") == 0) {

            putchar('\n'); fputs(HELPMSG_NAME                , stdout);
            putchar('\n'); fputs(HELPMSG_USAGE               , stdout);
            putchar('\n'); fputs(HELPMSG_OPTIONS_PT1         , stdout);
                           fputs(HELPMSG_OPTIONS_PT2         , stdout);
            putchar('\n'); fputs(HELPMSG_KEYS_COMMON         , stdout);
            putchar('\n'); fputs(HELPMSG_KEYS_SIM            , stdout);
            putchar('\n'); fputs(HELPMSG_KEYS_EDIT           , stdout);
            putchar('\n'); fputs(HELPMSG_KEYS_TEMPLATE       , stdout);
            putchar('\n'); fputs(HELPMSG_RULE_SYNTAX         , stdout);
            putchar('\n'); fputs(HELPMSG_RULE_EXAMPLE        , stdout);
            putchar('\n'); fputs(HELPMSG_TEMPLATE_SYNTAX     , stdout);
            putchar('\n'); fputs(HELPMSG_TEMPLATE_EXAMPLE_PT1, stdout);
            putchar('\n'); fputs(HELPMSG_TEMPLATE_EXAMPLE_PT2, stdout);
                           fputs(HELPMSG_TEMPLATE_EXAMPLE_PT3, stdout);
            putchar('\n'); fputs(HELPMSG_COLORS_PT1          , stdout);
                           fputs("    "COLORFACE_R0_WH       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R1_WH       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R2_WH       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R3_WH       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R4_WH       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R5_WH       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R0_BL       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R1_BL       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R2_BL       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R3_BL       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R4_BL       , stdout); puts(ESC"0m");
                           fputs("    "COLORFACE_R5_BL       , stdout); puts(ESC"0m");
                           fputs(HELPMSG_COLORS_PT2          , stdout);
            putchar('\n'); return 0; /* <- premature exit */

        } else if (strcmp(opt, "-a") == 0 || strcmp(opt, "--autofit") == 0) {
            if ( width_is_set) error_msg( "width value has already been set");
            if (height_is_set) error_msg("height value has already been set");
            width_is_set = height_is_set = true;

            if (!get_console_size(&width, &height))
                error_msg("couldn't get console size");
            width = width / 2 - 1;
            height = height - 2;
            if (width < MIN_FIELD_WIDTH || width > MAX_FIELD_WIDTH)
                error_msg("incorrect value for width");
            if (height < MIN_FIELD_HEIGHT || height > MAX_FIELD_HEIGHT)
                error_msg("incorrect value for height");

            unshift_arg();
        } else if (strcmp(opt, "-r") == 0 || strcmp(opt, "--rule") == 0) {
            if (!arg) error_msg("not enough arguments for option");
            if (rule_is_set) error_msg("rule value has already been set");
            rule_is_set = true;

            bsmask = parse_rule(arg, &gens);
            if (bsmask == INVALID_BS_MASK) goto error;
            normalization_rule(rule, bsmask, gens);
        } else if (strcmp(opt, "-p") == 0 || strcmp(opt, "--probability") == 0) {
            if (!arg) error_msg("not enough arguments for option");
            if (prob_is_set) error_msg("probability value has already been set");
            prob_is_set = true;

            prob = strtod(arg, &end);
            if (*end != '\0' || prob != prob || prob <= 0. || prob >= 1.)
                error_msg("incorrect value for probability");
        } else if (strcmp(opt, "-w") == 0 || strcmp(opt, "--width") == 0) {
            if (!arg) error_msg("not enough arguments for option");
            if (width_is_set) error_msg("width value has already been set");
            width_is_set = true;

            width = strtoul(arg, &end, 10);
            if (*end != '\0' || width < MIN_FIELD_WIDTH || width > MAX_FIELD_WIDTH)
                error_msg("incorrect value for width");
        } else if (strcmp(opt, "-h") == 0 || strcmp(opt, "--height") == 0) {
            if (!arg) error_msg("not enough arguments for option");
            if (height_is_set) error_msg("height value has already been set");
            height_is_set = true;

            height = strtoul(arg, &end, 10);
            if (*end != '\0' || height < MIN_FIELD_HEIGHT || height > MAX_FIELD_HEIGHT)
                error_msg("incorrect value for height");
        } else if (strcmp(opt, "-i") == 0 || strcmp(opt, "--indent") == 0) {
            if (!arg) error_msg("not enough arguments for option");
            if (indent_is_set) error_msg("indent value has already been set");
            indent_is_set = true;

            indent = strtoul(arg, &end, 10);
            if (*end != '\0') error_msg("incorrect value for indent");
        } else if (strcmp(opt, "-c") == 0 || strcmp(opt, "--colors") == 0) {
            ulong color_id; i = 0;
            if (!arg) error_msg("not enough arguments for option");
            if (colors_is_set) error_msg("color values has already been set");
            colors_is_set = true;

            do {
                if (i > 255) error_msg("too many colors");

                while (isspace(*arg)) ++arg;
                for (color_id = 0; isdigit(*arg); arg++)
                    color_id = 10 * color_id + (*arg - '0');

                if (*arg != '\0' && !isspace(*arg))
                    error_msgf("unexpected character '%c' in color list", *arg);
                if (color_id > 255) error_msgf("incorrect id '%lu' for color", color_id);

                sprintf(state_colors[i++], ESC"38;5;%lum", color_id);
            } while (*arg);

            if (i < 2) error_msg("not enough colors");
            state_colors[0][2] = '4';
        } else if (opt[0] == '-' && ('1' <= opt[1] && opt[1] <= '9') && opt[2] == '\0') {
            ulong slot_index = opt[1] - '1';
            if (!arg) error_msg("not enough arguments for option");
            if (template_slots[slot_index].array)
                error_msgf("template in slot %lu has already been set", slot_index + 1);
            template_slots[slot_index].array = (void*)arg;
        } else
            error_msgf("expected option, but got '%s'", opt);
    }

    /* Set default value for parameters */
    if (!  prob_is_set) prob   = DEFAULT_PROB;
    if (! width_is_set) width  = DEFAULT_WIDTH;
    if (!height_is_set) height = DEFAULT_HEIGHT;
    if (!indent_is_set) indent = DEFAULT_INDENT;
    if (!  rule_is_set) {
        bsmask = parse_rule(DEFAULT_RULE, &gens);
        strcpy(rule, DEFAULT_RULE);
    }

    /* Checking colors for states */
    if (colors_is_set && strlen(state_colors[gens]) == 0)
        error_msg("not enough colors for states");

    /* Checking indent after get width and height */
    if (indent > width / 2 || indent > height / 2)
        error_msg("the indentation value is too high");

    /* Convert RLE string to template */
    for (i = 0; i < COUNT_TEMPLATE_SLOT; i++) {
        if (!template_slots[i].array) continue;
        template_slots[i] = parse_rle(
            (void*)template_slots[i].array, gens, width, height);
        if (!template_slots[i].array) goto error;
    }

    /* Allocation memory for fields */
    field_fst = malloc(width * (height + 1)); /* additional line for moving field */
    field_snd = malloc(width *  height     );
    if (!field_snd || !field_snd)
        error_msg("couldn't allocate memory");

    /* Setup terminal */
#if defined(__linux__)
    setup_terminal();
#else
    setvbuf(stdout, output_buffer, _IOFBF, sizeof output_buffer);
#endif

    fputs(ESC"?25l", stdout); /* hide cursor */

    /* Drawing border and information on screen */
    draw_border(width * 2, height);
    fputs(ESC"3G" LVER_BAR" "MODE_TXT_SIMULATION" "RVER_BAR, stdout);
    printf(ESC"1;3H" LVER_BAR INFOFMT RVER_BAR, rule, width, height, prob);

    srand(time(NULL));
restart:
    /* Initialization of fields */
    memset(field_fst, 0, width * height);
    memset(field_snd, 0, width * height);
    for (i = indent; i < height - indent; i++)
    for (j = indent; j < width  - indent; j++)
        FSTF(i, j) = randf0t1() < prob
            ? (full_alive_only ? gens : randrange(gens - 1) + 1) : 0;

    /* Main program loop */
    while (true) {
        fputs(ESC"2;2H", stdout); /* move to left-up cell */

        /* Drawing on screen */
        fputs(state_colors[0], stdout);
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                uchar cell = FSTF(i, j);
                bool has_cell = cell > 0;
                fputs(state_colors[cell], stdout);
                switch(mode) {
                    case MODE_SIMULATION: case MODE_PAUSE: case MODE_ONESTEP:
                        fputs(has_cell ? ALIVE_CELL_BLOCK : DEAD_CELL_BLOCK, stdout);
                        break;

                    case MODE_CURSOR:
                        if (cursor_x == j && cursor_y == i) {
                            fputs(state_colors[has_cell ? cell : gens], stdout);
                            fputs(has_cell ? ALIVE_CURSOR_BLOCK : DEAD_CURSOR_BLOCK, stdout);
                        } else
                            fputs(has_cell ? ALIVE_CELL_BLOCK : DEAD_CELL_BLOCK, stdout);
                        break;

                    case MODE_RECTANGLE:
                        if (rect_x <= j && j <= cursor_x && rect_y <= i && i <= cursor_y) {
                            fputs(state_colors[has_cell ? cell : gens], stdout);
                            fputs(has_cell ? ALIVE_CURSOR_BLOCK : DEAD_CURSOR_BLOCK, stdout);
                        } else
                            fputs(has_cell ? ALIVE_CELL_BLOCK : DEAD_CELL_BLOCK, stdout);
                        break;

                    case MODE_TEMPLATE_1: case MODE_TEMPLATE_2: case MODE_TEMPLATE_3:
                    case MODE_TEMPLATE_4: case MODE_TEMPLATE_5: case MODE_TEMPLATE_6:
                    case MODE_TEMPLATE_7: case MODE_TEMPLATE_8: case MODE_TEMPLATE_9:
                    case MODE_TEMPLATE_BUF: {
                        template_t* slot = template_slots + mode - MODE_TEMPLATE_1;
                        if (cursor_x <= j && j < cursor_x + slot->width &&
                            cursor_y <= i && i < cursor_y + slot->height) {
                            uchar hole = slot->array[slot->width * (i - cursor_y) + (j - cursor_x)];
                            fputs(state_colors[hole ? hole : gens], stdout);
                            fputs(hole > 0 ? ALIVE_CURSOR : DEAD_CURSOR, stdout);
                            fputs(state_colors[FSTF(i, j)], stdout);
                            fputs(has_cell ? ALIVE_CELL : DEAD_CELL, stdout);
                        } else
                            fputs(has_cell ? ALIVE_CELL_BLOCK : DEAD_CELL_BLOCK, stdout);
                    } break;
                }
            }
            fputs(ESC"2G" ESC"1B", stdout);
        }
        fputs(ESC"0m", stdout);

        /* Handling pressing keys */
        if (mode == MODE_ONESTEP) mode = MODE_PAUSE;
        if (is_symbol_received()) {
            int key = received_symbol();

            /* Exit from main loop */
            if (key == 'q') {
                printf(ESC"%lu;1H\n", height + 2);
                break;
            }

            switch (mode) {
                /* Simulation mode */
                case MODE_SIMULATION:
                    /**/ if (key == 'p') { mode = MODE_PAUSE; PUT_BAR_PAUSE; }
                    goto common_SIM_and_PAUSE;
                case MODE_PAUSE:
                    /**/ if (key == 'p') { mode = MODE_SIMULATION; PUT_BAR_SIMULATION; }
                    else if (key == 'o')   mode = MODE_ONESTEP;
                    goto common_SIM_and_PAUSE;
                case MODE_ONESTEP: /* nothing */ break;
                common_SIM_and_PAUSE:
                    switch (key) {
                        case 'r': full_alive_only = false; goto restart;
                        case 'R': full_alive_only =  true; goto restart;
                        case 'w': move_to_up   (field_fst, width, height); break;
                        case 's': move_to_down (field_fst, width, height); break;
                        case 'a': move_to_left (field_fst, width, height); break;
                        case 'd': move_to_right(field_fst, width, height); break;
                        case 'e': {
                            cursor_x = width  / 2;
                            cursor_y = height / 2;
                            rect_x = rect_y = 0;

                            mode = MODE_CURSOR;
                            PUT_BAR_CURSOR;
                        } break;
                    } break;

                /* Edit mode */
                case MODE_CURSOR:
                    /*  */ if (key == '0' && template_slots[9].array) {
                        PUT_BAR_TEMPLATE_BUF; mode = MODE_TEMPLATE_BUF;
                        cursor_x = min(cursor_x,  width - template_slots[9]. width);
                        cursor_y = min(cursor_y, height - template_slots[9].height);
                    } else if ('1' <= key && key <= '9') {
                        ulong index = key - '1';
                        if (template_slots[index].array) {
                            PUT_BAR_TEMPLATE; mode = MODE_TEMPLATE_1 + index;
                            cursor_x = min(cursor_x,  width - template_slots[index]. width);
                            cursor_y = min(cursor_y, height - template_slots[index].height);
                        }
                    }

                    else if (key == 'r') { mode = MODE_RECTANGLE; rect_x = cursor_x; rect_y = cursor_y; }

                    else if (key == 'g') FSTF(cursor_y, cursor_x) = 0;
                    else if (key == 'b') FSTF(cursor_y, cursor_x) = gens;
                    else if (key == 't') FSTF(cursor_y, cursor_x) = gens - FSTF(cursor_y, cursor_x);

                    goto common_CUR_and_RECT;
                case MODE_RECTANGLE:
                    /**/ if (key == 'r') { mode = MODE_CURSOR; rect_x = rect_y = 0; }

                    else if (key == 'g')
                        for (i = rect_y; i <= cursor_y; i++)
                        for (j = rect_x; j <= cursor_x; j++)
                            FSTF(i, j) = 0;
                    else if (key == 'b')
                        for (i = rect_y; i <= cursor_y; i++)
                        for (j = rect_x; j <= cursor_x; j++)
                            FSTF(i, j) = gens;
                    else if (key == 't')
                        for (i = rect_y; i <= cursor_y; i++)
                        for (j = rect_x; j <= cursor_x; j++)
                            FSTF(i, j) = gens - FSTF(i, j);

                    else if (key == 'c' || key == 'x') {
                        size_t rect_w = cursor_x - rect_x + 1;
                        size_t rect_h = cursor_y - rect_y + 1;
                        template_t* slot = template_slots + 9;
                        uchar* newptr = realloc(slot->array, rect_w * rect_h);
                        if (newptr) {
                            slot->array = newptr;
                            slot->width  = rect_w;
                            slot->height = rect_h;
                            for (i = rect_y; i <= cursor_y; i++)
                            for (j = rect_x; j <= cursor_x; j++) {
                                slot->array[slot->width * (i - rect_y) + (j - rect_x)] = FSTF(i, j);
                                if (key == 'x') FSTF(i, j) = 0;
                            }
                        }
                    }

                    goto common_CUR_and_RECT;
                common_CUR_and_RECT:
                    switch (key) {
                        case 'w': if (rect_y < cursor_y    ) { cursor_y -= 1; } break;
                        case 's': if (cursor_y < height - 1) { cursor_y += 1; } break;
                        case 'a': if (rect_x < cursor_x    ) { cursor_x -= 1; } break;
                        case 'd': if (cursor_x < width  - 1) { cursor_x += 1; } break;

                        case 'W': if (rect_y + 10 <= cursor_y) { cursor_y -= 10; } break;
                        case 'S': if (cursor_y < height - 10 ) { cursor_y += 10; } break;
                        case 'A': if (rect_x + 10 <= cursor_x) { cursor_x -= 10; } break;
                        case 'D': if (cursor_x < width  - 10 ) { cursor_x += 10; } break;

                        case 'C':
                            memset(field_fst, 0, width * height);
                            memset(field_snd, 0, width * height);
                            break;

                        case 'e': mode = MODE_PAUSE; PUT_BAR_PAUSE; break;
                    }
                    /**/ if (mode == MODE_CURSOR) PUT_BAR_CURSOR;
                    else if (mode == MODE_RECTANGLE) PUT_BAR_RECTANGLE;
                    break;

                /* Template mode */
                case MODE_TEMPLATE_1: case MODE_TEMPLATE_2: case MODE_TEMPLATE_3:
                case MODE_TEMPLATE_4: case MODE_TEMPLATE_5: case MODE_TEMPLATE_6:
                case MODE_TEMPLATE_7: case MODE_TEMPLATE_8: case MODE_TEMPLATE_9:
                case MODE_TEMPLATE_BUF: {
                    template_t* slot = template_slots + mode - MODE_TEMPLATE_1;
                    switch (key) {
                        case 'w': if (0 < cursor_y                    ) { cursor_y -= 1; } break;
                        case 's': if (cursor_y < height - slot->height) { cursor_y += 1; } break;
                        case 'a': if (0 < cursor_x                    ) { cursor_x -= 1; } break;
                        case 'd': if (cursor_x <  width - slot-> width) { cursor_x += 1; } break;

                        case 'f': flip_horizontally(slot); break;
                        case 'F': flip_vertically  (slot); break;
                        case 'g': rotate_by_180deg (slot); break;

                        case 'p':
                            for (i = cursor_y; i < cursor_y + slot->height; i++)
                            for (j = cursor_x; j < cursor_x + slot-> width; j++)
                                FSTF(i, j) = slot->array[slot->width * (i - cursor_y) + (j - cursor_x)];
                            break;
                        case 'o':
                            for (i = cursor_y; i < cursor_y + slot->height; i++)
                            for (j = cursor_x; j < cursor_x + slot-> width; j++)
                                if (slot->array[slot->width * (i - cursor_y) + (j - cursor_x)] > 0)
                                    FSTF(i, j) = slot->array[slot->width * (i - cursor_y) + (j - cursor_x)];
                            break;

                        case 'e': mode = MODE_CURSOR; PUT_BAR_CURSOR; break;
                    } break;
                } break;
            }
        }

        /* Update current field */
        if (mode == MODE_SIMULATION || mode == MODE_ONESTEP) {
            for (i = 0; i < height; i++)
            for (j = 0; j <  width; j++)
                if (FSTF(i, j) == 0 || FSTF(i, j) == gens) {
                    ulong cnt = 0;
                    size_t iu = prev_size_t(i, height);
                    size_t id = next_size_t(i, height);
                    size_t jl = prev_size_t(j,  width);
                    size_t jr = next_size_t(j,  width);

                    cnt += FSTF(iu, jl) == gens; cnt += FSTF(iu, j) == gens; cnt += FSTF(iu, jr) == gens;
                    cnt += FSTF(i , jl) == gens;                             cnt += FSTF(i , jr) == gens;
                    cnt += FSTF(id, jl) == gens; cnt += FSTF(id, j) == gens; cnt += FSTF(id, jr) == gens;

                    if (FSTF(i, j) == gens)
                        SNDF(i, j) = FSTF(i, j) - ((bsmask & (1ul << (cnt + 9))) == 0);
                    else
                        SNDF(i, j) = bsmask & (1ul << cnt) ? gens : 0;
                } else
                    SNDF(i, j) = FSTF(i, j) - 1;
            memcpy(field_fst, field_snd, width * height);
        }

        /* Update screen */
        fflush(stdout); sleep_ms(DELAY_IN_MILLISECOND);
    }

    fputs(ESC"?25h", stdout); /* show cursor */

    rc = EXIT_SUCCESS;
error:
    for (i = 0; i < COUNT_TEMPLATE_SLOT; i++)
        if (template_slots[i].width || template_slots[i].height)
            free(template_slots[i].array);
    free(field_fst);
    free(field_snd);
    return rc;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                      Implementation support functions                     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

float randf0t1(void) { return (float)rand() / (float)RAND_MAX; }

uchar randrange(uchar max) { return rand() % (max + 1); }

size_t prev_size_t(size_t value, ulong len) {
    if (value == 0) return len - 1; else return value - 1;
}

size_t next_size_t(size_t value, ulong len) {
    if (value == len - 1) return 0; else return value + 1;
}

ulong parse_rule(const char* str, uchar* gens) {
    ulong mask = 0, digit, number;

    /* Parsing birth digits */
    if (*str != 'B' && *str != 'b')
        error_msgf("expected start birth number, but got '%c'", *str);
    else ++str;

    for (; *str != '/' && *str != '\0'; ++str) {
        if (*str < '0' || *str > '8')
            error_msgf("expected digit less than 9, but got '%c'", *str);
        digit = *str - '0';
        if ((mask >> digit) & 1)
            error_msgf("'%c' already set", *str);
        mask |= 1ul << digit;
    }

    if (*str != '/')
        error_msgf("expected rule delimiter, but got '%c'", *str);
    else ++str;

    /* Parsing survive digits */
    if (*str != 'S' && *str != 's')
        error_msgf("expected start survive number, but got '%c'", *str);
    else ++str;

    for (; *str != '/' && *str != '\0'; ++str) {
        if (*str < '0' || *str > '8')
            error_msgf("expected digit less than 9, but got '%c'", *str);
        digit = *str - '0';
        if ((mask >> (digit + 9)) & 1)
            error_msgf("'%c' already set", *str);
        mask |= 1ul << (digit + 9);
    }

    /* Parsing count generations */
    if (*str != '/' && *str != '\0')
        error_msgf("expected rule delimiter or end, but got '%c'", *str);
    else if (*str == '/') {
        ++str;
        if (*str != 'G' && *str != 'g')
            error_msgf("expected start generation count, but got '%c'", *str);
        ++str;
        for (number = 0; *str != '\0'; ++str) {
            if (*str < '0' || *str > '9')
                error_msgf("expected digit, but got '%c'", *str);
            number = number * 10 + *str - '0';
        }
        if (number < 2 || number > 256)
            error_msg("generation count too high or low");
        *gens = number - 1;
    } else
        *gens = 1; /* i.e. 2 states */

    return mask;
error:
    return INVALID_BS_MASK;
}

void normalization_rule(char* rule, ulong mask, uchar gens) {
    int i;

    *rule++ = 'B';
    for (i = 0; i < 9; i++, mask >>= 1)
        if (mask & 1) *rule++ = '0' + i;
    *rule++ = '/';

    *rule++ = 'S';
    for (i = 0; i < 9; i++, mask >>= 1)
        if (mask & 1) *rule++ = '0' + i;
    *rule++ = '/';

    *rule++ = 'G';
    sprintf(rule, "%i", (int)gens + 1);
}

template_t parse_rle(const char* rle, uchar gens, ulong width, ulong height) {
    template_t new = {0}; char* end;
    ulong x = 0, y = 0;

    new.width = strtoul(rle, &end, 10);
    if (*end != ':') error_msgf("unexpected character '%c' after width", *end);
    new.height = strtoul(end + 1, &end, 10);
    if (*end != ':') error_msgf("unexpected character '%c' after height", *end);

    if (new.width  == 0 || new.width > width)
        error_msg("incorrect value for template width");
    if (new.height == 0 || new.height > height)
        error_msg("incorrect value for template height");

    new.array = malloc(new.width * new.height);
    if (!new.array) error_msg("couldn't allocate memory");
    memset(new.array, 0, new.width * new.height);

    for (rle = end + 1; *rle != '!'; rle++) {
        ulong len = strtoul(rle, &end, 10);
        if (end != rle) {
            if (len == 0) error_msg("zero count for tag in rle");
            else rle = end;
        } else
            len = 1;

        switch (*rle) {
            case 'b': case 'o':
                if (x + len > new.width)
                    error_msg("the tag count is too high");
                while (len --> 0)
                    new.array[new.width * y + (x++)] = *rle == 'o' ? gens : 0;
            break;

            case '$':
                if (y + len > new.height)
                    error_msg("the tag count is too high");
                y += len; x = 0;
            break;

            case  ' ': case '\r': case '\n':
            case '\t': case '\v': case '\f':
                /* skip whitespaces */
            break;

            case '\0': error_msg("unexpected end of RLE");
            default: error_msgf("unexpected tag '%c' in rle", *rle);
        }
    }

    return new;
error:
    free(new.array);
    memset(&new, 0, sizeof new);
    return new;
}

void draw_border(ulong w, ulong h) {
    ulong i;

    fputs(ESC"H" , stdout); /* move to start */
    fputs(ESC"2J", stdout); /* clear screen */

    fputs(LU_CORNER, stdout);
    for (i = w; i > 0; i -= min(i, MIN_FIELD_WIDTH))
        fwrite(HOR_BAR_LINE, min(i, MIN_FIELD_WIDTH) * strlitlen(HOR_BAR), 1, stdout);
    fputs(RU_CORNER"\n", stdout);

    for (i = 0; i < h; i++)
        printf(VER_BAR ESC"%luC" VER_BAR "\n", w);

    fputs(LD_CORNER, stdout);
    for (i = w; i > 0; i -= min(i, MIN_FIELD_WIDTH))
        fwrite(HOR_BAR_LINE, min(i, MIN_FIELD_WIDTH) * strlitlen(HOR_BAR), 1, stdout);
    fputs(RD_CORNER, stdout);
}

void move_to_up(uchar* field, size_t width, size_t heigth) {
    memmove(field + width, field, width * heigth);
    memcpy(field, field + width * heigth, width);
}

void move_to_down(uchar* field, size_t width, size_t heigth) {
    memcpy(field + width * heigth, field, width);
    memmove(field, field + width, width * heigth);
}

void move_to_left(uchar* field, size_t width, size_t heigth) {
    size_t i; for (i = 0; i < heigth; i++) {
        uchar right = field[width * i + width - 1];
        memmove(field + width * i + 1, field + width * i, width - 1);
        field[width * i] = right;
    }
}

void move_to_right(uchar* field, size_t width, size_t heigth) {
    size_t i; for (i = 0; i < heigth; i++) {
        uchar left = field[width * i];
        memmove(field + width * i, field + width * i + 1, width - 1);
        field[width * i + width - 1] = left;
    }
}

void flip_horizontally(template_t* tmpl) {
    size_t i; for (i = 0; i < tmpl->height; i++) {
        uchar* first = tmpl->array + tmpl->width * i;
        uchar* last  = tmpl->array + tmpl->width * (i + 1) - 1;
        for (; first < last; first++, last--) {
            uchar t = *first; *first = *last; *last = t;
        }
    }
}

void flip_vertically(template_t* tmpl) {
    uchar* first = tmpl->array;
    uchar* last  = tmpl->array + tmpl->width * (tmpl->height - 1);
    for (; first < last; first += tmpl->width, last -= tmpl->width) {
        size_t i; uchar t;
        for (i = 0; i < tmpl->width; i++) {
            t = first[i]; first[i] = last[i]; last[i] = t;
        }
    }
}

void rotate_by_180deg(template_t* tmpl) {
    size_t i, N = tmpl->width * tmpl->height;
    for (i = 0; i < N / 2; i++) {
        uchar t = tmpl->array[i];
        tmpl->array[i] = tmpl->array[N - i - 1];
        tmpl->array[N - i - 1] = t;
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                           OS dependent functions                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if defined(_WIN32)

#include <windows.h>
#include <conio.h>

bool get_console_size(ulong* width, ulong* height) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int ret = GetConsoleScreenBufferInfo(
        GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    if (ret) {
        * width = csbi.dwSize.X;
        *height = csbi.dwSize.Y;
        return true;
    } else
        return false;
}

void sleep_ms(ulong ms) { Sleep(ms); }

bool is_symbol_received(void) { return kbhit(); }

int received_symbol(void) { return getch(); }

#elif defined(__linux__)
/* Terminal setup: https://stackoverflow.com/a/63708756 */

void reset_terminal(void) {
    tcsetattr(0, TCSANOW, &orig_termios);
}

void setup_terminal(void) {
    atexit(reset_terminal);

    tcgetattr(0, &orig_termios);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~ICANON;
    new_termios.c_lflag &= ~ECHO;
    new_termios.c_lflag &= ~ISIG;
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_termios);
}

bool get_console_size(ulong* width, ulong* height) {
    struct winsize ws;
    if (!ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)) {
        * width = ws.ws_col;
        *height = ws.ws_row;
        return true;
    } else
        return false;
}

void sleep_ms(ulong ms) { usleep(ms * 1000); }

bool is_symbol_received(void) {
    int n; unsigned char ch;

    if (peek_char >= 0) return true;

    new_termios.c_cc[VMIN] = 0;
    tcsetattr(0, TCSANOW, &new_termios);
    n = read(0, &ch, 1);
    new_termios.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new_termios);

    if (n == 1) {
        peek_char = ch;
        return true;
    } else
        return false;
}

int received_symbol(void) {
    char ch;
    if (peek_char >= 0) {
        ch = peek_char;
        peek_char = -1;
    } else
        read(0, &ch, 1);
    return ch;
}

#else
#  error Unsupported operation system
#endif