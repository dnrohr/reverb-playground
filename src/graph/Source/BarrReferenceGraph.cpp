#include <reverb/graph/BarrReferenceGraph.h>

namespace reverb::graph {
namespace {

Port input(const std::string& id) { return { id, SignalType::audio, PortDirection::input }; }
Port output(const std::string& id) { return { id, SignalType::audio, PortDirection::output }; }
Parameter milliseconds(const double value) { return { "delay", value, "milliseconds" }; }
Connection connect(const std::string& id, const std::string& from, const std::string& to, const std::string& toPort = "in")
{
    return { id, { from, "out" }, { to, toPort } };
}

} // namespace

GraphDocument makeBarrReferenceGraph()
{
    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { output("out-l"), output("out-r") }, {} },
        { "sum", "sum", { input("in-l"), input("in-r"), output("out") }, {} },
        { "input-filter", "lowpass", { input("in"), output("out") }, { { "cutoff", 7'000.0, "hertz" } } },
        { "diffuser-1", "allpass", { input("in"), output("out") }, { milliseconds(4.31), { "coefficient", 0.5, "unitless" } } },
        { "diffuser-2", "allpass", { input("in"), output("out") }, { milliseconds(7.13), { "coefficient", 0.5, "unitless" } } },
        { "tank-1", "allpass", { input("in"), output("out") }, { milliseconds(13.73), { "coefficient", 0.5, "unitless" } } },
        { "tank-2", "allpass", { input("in"), output("out") }, { milliseconds(19.91), { "coefficient", -0.5, "unitless" } } },
        { "left-tap", "allpass", { input("in"), output("out") }, { milliseconds(29.71), { "coefficient", 0.5, "unitless" } } },
        { "right-tap", "allpass", { input("in"), output("out") }, { milliseconds(37.11), { "coefficient", 0.5, "unitless" } } },
        { "output", "stereo-output", { input("in-l"), input("in-r") }, {} },
    };
    graph.connections = {
        { "input-l-to-sum", { "input", "out-l" }, { "sum", "in-l" } },
        { "input-r-to-sum", { "input", "out-r" }, { "sum", "in-r" } },
        connect("sum-to-filter", "sum", "input-filter"),
        connect("filter-to-diffuser-1", "input-filter", "diffuser-1"),
        connect("diffuser-1-to-diffuser-2", "diffuser-1", "diffuser-2"),
        connect("diffuser-2-to-tank-1", "diffuser-2", "tank-1"),
        connect("tank-1-to-tank-2", "tank-1", "tank-2"),
        connect("tank-to-left", "tank-2", "left-tap"),
        connect("tank-to-right", "tank-2", "right-tap"),
        connect("left-to-output", "left-tap", "output", "in-l"),
        connect("right-to-output", "right-tap", "output", "in-r"),
    };
    graph.layout.nodes = {
        { "input", 0.0, 100.0 }, { "sum", 180.0, 100.0 }, { "input-filter", 360.0, 100.0 },
        { "diffuser-1", 540.0, 100.0 }, { "diffuser-2", 720.0, 100.0 },
        { "tank-1", 900.0, 100.0 }, { "tank-2", 1'080.0, 100.0 },
        { "left-tap", 1'260.0, 40.0 }, { "right-tap", 1'260.0, 160.0 }, { "output", 1'440.0, 100.0 },
    };
    return graph;
}

} // namespace reverb::graph
