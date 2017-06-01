#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>

typedef std::string Path;

enum class State : unsigned int
{
	Zero = 0,
	One = 1,
	Unknown = 2,
	Faulty = 3,
};

const char* stateToString(State state)
{
	switch (state)
	{
	case State::Zero:
		return "State::Zero";

	case State::One:
		return "State::One";

	default:
	case State::Unknown:
		return "State::Unknown";

	case State::Faulty:
		return "State::Faulty";
	}
}

struct Node
{
	Node(State input = State::Faulty, State output = State::Faulty)
	: inputValue(input)
	, outputValue(output)
	{
	};

	State inputValue;
	State outputValue;
};

class Traits
{
public:
	Traits(int source, int m, int n, bool debug = false)
	: m_source(source)
	, m_m(m)
	, m_n(n)
	, m_debug(debug)
	{
	}

	Node getSourceValue() const
	{
		return Node(State::Zero, State::Unknown);
	}

	State getValue(State value, int source, int destination, const Path& path) const
	{
		if (source == m_source)
		{
			return (destination & 1) ? State::Zero : State::One;
		}
		else if (source == 2)
		{
			return State::One;
		}
		
		return value;
	}

	State getDefault() const
	{
		return State::One;
	}

	bool isFaulty(int process) const
	{
		return (process == m_source || process == 2);
	}

	const int m_source;
	const int m_m;
	const size_t m_n;
	const bool m_debug;
};

class Process
{
public:
	Process(int id)
	: m_processId(id)
	{
		if (ms_children.size() == 0)
			generateChildren(ms_traits.m_m, ms_traits.m_n, std::vector<bool>(ms_traits.m_n, true));
		
		if (m_processId == ms_traits.m_source)
			m_nodes[""] = ms_traits.getSourceValue();
	}

	void sendMessages(int round, std::vector<Process> &processes)
	{
		for (size_t i = 0; i < ms_pathsByRank[round][m_processId].size(); i++)
		{
			Path sourceNodePath = ms_pathsByRank[round][m_processId][i];
			sourceNodePath = sourceNodePath.substr(0, sourceNodePath.size() - 1);
			
			const Node& sourceNode = m_nodes[sourceNodePath];
			
			for (size_t j = 0; j < ms_traits.m_n; j++)
			{
				if (j != ms_traits.m_source)
				{
					State value = ms_traits.getValue(sourceNode.inputValue, m_processId, (int)j, ms_pathsByRank[round][m_processId][i]);

					if (ms_traits.m_debug)
					{
						std::cout << "Sending from process " << m_processId
							<< " to " << static_cast<unsigned int>(j)
							<< ": {" << stateToString(value) << ", "
							<< ms_pathsByRank[round][m_processId][i]
							<< ", " << stateToString(State::Unknown) << "}"
							<< ", getting value from source_node " << sourceNodePath
							<< "\n";
					}

					processes[j].receiveMessage(ms_pathsByRank[round][m_processId][i], Node(value, State::Unknown));
				}
			}
		}
	}

	State decide()
	{
		if (m_processId == ms_traits.m_source)
		{
			const Node& node = m_nodes[""];
			return node.inputValue;
		}

		for (size_t i = 0; i < ms_traits.m_n; i++)
		{
			for (size_t j = 0; j < ms_pathsByRank[ms_traits.m_m][i].size(); j++)
			{
				Path path = ms_pathsByRank[ms_traits.m_m][i][j];
				Node& node = m_nodes[path];
				node.outputValue = node.inputValue;
			}
		}

		for (int round = (int)ms_traits.m_m - 1; round >= 0; round--)
		{
			for (size_t i = 0; i < ms_traits.m_n; i++)
			{
				for (size_t j = 0; j < ms_pathsByRank[round][i].size(); j++)
				{
					Path path = ms_pathsByRank[round][i][j];
					Node& node = m_nodes[path];
					node.outputValue = getMajority(path);
				}
			}
		}

		const Path& topPath = ms_pathsByRank[0][ms_traits.m_source].front();
		const Node& topNode = m_nodes[topPath];
		return topNode.outputValue;
	}

	std::string dump(Path path = "")
	{
		if (path == "")
			path = ms_pathsByRank[0][ms_traits.m_source].front();
		
		std::stringstream s;
		for (size_t i = 0; i < ms_children[path].size(); i++)
			s << dump(ms_children[path][i]);
		
		const Node& node = m_nodes[path];
		s << "{" << stateToString(node.inputValue)
			<< "," << path
			<< "," << stateToString(node.outputValue)
			<< "}\n";
		
		return s.str();
	}

	std::string dumpDot(Path path = "")
	{
		bool root = false;
		std::stringstream s;
		if (path == "")
		{
			root = true;
			path = ms_pathsByRank[0][ms_traits.m_source].front();
			s << "digraph byz {\n"
				<< "rankdir=LR;\n"
				<< "nodesep=.0025;\n"
				<< "label=\"Process " << m_processId << "\";\n"
				<< "node [fontsize=8,width=.005,height=.005,shape=plaintext];\n"
				<< "edge [fontsize=8,arrowsize=0.25];\n";
		}

		Node& node = m_nodes[path];
		for (size_t i = 0; i < ms_children[path].size(); i++)
			s << dumpDot(ms_children[path][i]);
		
		if (path.size() == 1)
		{
			s << "General->";
		}
		else
		{
			Path parentPath = path.substr(0, path.size() - 1);
			Node& parentNode = m_nodes[parentPath];
			s << "\"{" << stateToString(parentNode.inputValue)
				<< "," << parentPath
				<< "," << stateToString(parentNode.outputValue)
				<< "}\"->";
		}

		s << "\"{" << stateToString(node.inputValue)
			<< "," << path
			<< "," << stateToString(node.outputValue)
			<< "}\";\n";
		
		if (root)
			s << "};\n";
		
		return s.str();
	}

	bool isFaulty() const
	{
		return ms_traits.isFaulty(m_processId);
	}

	bool isSource() const
	{
		return ms_traits.m_source == m_processId;
	}

private:
	int m_processId;

	// Map holding the process tree
	std::map<Path, Node> m_nodes;

	// Static data shared among all process objects
	static Traits ms_traits;
	static std::map<Path, std::vector<Path>> ms_children;
	static std::map<size_t, std::map<size_t, std::vector<Path>>> ms_pathsByRank;

	State getMajority(const Path& path)
	{
		std::map<State, size_t> counts;
		counts[State::One] = 0;
		counts[State::Zero] = 0;
		counts[State::Unknown] = 0;
		
		size_t n = ms_children[path].size();
		for (size_t i = 0; i < n; i++)
		{
			const Path& child = ms_children[path][i];
			const Node& node  = m_nodes[child];
			counts[node.outputValue]++;
		}

		if (counts[State::One] > (n / 2))
			return State::One;

		if (counts[State::Zero] > (n / 2))
			return State::Zero;

		if (counts[State::One] == counts[State::Zero] && counts[State::One] == (n / 2))
			return ms_traits.getDefault();
		
		return State::Unknown;
	}

	void receiveMessage(const Path& path, const Node& node)
	{
		m_nodes[path] = node;
	}

	static void generateChildren(
		const size_t m,
		const size_t n,
		std::vector<bool> ids,
		int source = ms_traits.m_source,
		Path currentPath = "",
		size_t rank = 0)
	{
		ids[source] = false;
		currentPath += static_cast<char>(source + '0');
		ms_pathsByRank[rank][source].push_back(currentPath);
		
		if (rank < m)
		{
			for (int i = 0; i < static_cast<int>(ids.size()); i++)
			{
				if (ids[i])
				{
					generateChildren(m, n, ids, i, currentPath, rank + 1);
					ms_children[currentPath].push_back(currentPath + static_cast<char>(i + '0'));
				}
			}
		}

		if (ms_traits.m_debug)
		{
			std::cout << currentPath << ", children = ";
			
			for (size_t j = 0; j < ms_children[currentPath].size(); j++)
			{
				std::cout << ms_children[currentPath][j] << " ";
			}

			std::cout << "\n";
		}
	}
};


const int N = 7;
const int M = 2;
const int SOURCE = 3;
const bool DEBUG = true;

std::map<Path, std::vector<Path>> Process::ms_children;
std::map<size_t, std::map<size_t, std::vector<Path>>> Process::ms_pathsByRank;
Traits Process::ms_traits = Traits(SOURCE, M, N, DEBUG);

int main()
{
	std::vector<Process> processes;
	for (int i = 0; i < N; i++)
	{
		processes.push_back(Process(i));
	}

	for (int i = 0; i <= M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			processes[j].sendMessages(i, processes);
		}
	}

	for (int j = 0; j < N; j++)
	{
		if (processes[j].isSource())
			std::cout << "Source ";
		
		std::cout << "Process " << j;
		
		if (!processes[j].isFaulty())
		{
			State choice = processes[j].decide();
			std::cout << " decides on value " << stateToString(choice);
		}
		else
		{
			std::cout << " is faulty";
		}

		std::cout << "\n";
	}

	std::cout << "\n";
	for (;;)
	{
		std::string s;
		std::cout << "Specify id of process to generate .dot schema, or press enter to quit: ";
		getline(std::cin, s);
		if (s.size() == 0)
			break;

		int id;
		std::stringstream s1(s);
		s1 >> id;

		if (DEBUG)
		{
			std::cout << processes[id].dump() << "\n";
			getline(std::cin, s);
		}

		std::cout << processes[id].dumpDot() << "\n";
	}

	return 0;
}
