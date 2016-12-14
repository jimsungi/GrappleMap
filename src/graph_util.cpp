#include "graph_util.hpp"

namespace GrappleMap {

SeqNum insert(Graph & g, Sequence const & sequence)
{
	SeqNum const num{g.num_sequences()};
	g.set(none, sequence);
	return num;
}

optional<SeqNum> erase_sequence(Graph & g, SeqNum const sn)
{
	if (g.num_sequences() == 1)
	{
		std::cerr << "Cannot erase last sequence." << std::endl;
		return none;
	}

	auto const & seq = g[sn];

	std::cerr <<
		"Erasing sequence " << sn.index <<
		" (\"" << seq.description.front() << "\")"
		" and the " << seq.positions.size() << " positions in it." << std::endl;

	g.set(sn, none);

	return SeqNum{sn.index == 0 ? 0 : sn.index - 1};
}

optional<Step> step_by_desc(Graph const & g, string const & desc, optional<NodeNum> const from)
{
	foreach(sn : seqnums(g))
		if (replace_all(g[sn].description.front(), "\\n", " ") == desc
			|| desc == "t" + std::to_string(sn.index))
		{
			if (!from || *g.from(sn) == *from)
				return Step{sn, false};
			if (is_bidirectional(g[sn]) && *g.to(sn) == *from)
				return Step{sn, true};
		}

	return none;
}

optional<NodeNum> node_by_desc(Graph const & g, string const & desc)
{
	if (desc.size() >= 2 && desc.front() == 'p' && all_digits(desc.substr(1)))
		return NodeNum{uint16_t(std::stoul(desc.substr(1)))};

	foreach(n : nodenums(g))
	{
		auto const & d = g[n].description;
		if (!d.empty() && replace_all(d.front(), "\\n", " ") == desc)
			return n;
	}

	return none;
}

optional<PositionInSequence> posinseq_by_desc(Graph const & g, string const & s)
{
	if (s == "last-trans") return first_pos_in({g.num_sequences() - 1u});

	if (auto step = step_by_desc(g, s)) return first_pos_in(**step);

	if (auto n = node_by_desc(g, s)) return node_as_posinseq(g, *n);

	return none;
}

optional<PositionInSequence> node_as_posinseq(Graph const & g, NodeNum const node)
{
	foreach(sn : seqnums(g)) // todo: bad
		if (*g.from(sn) == node) return first_pos_in(sn);
		else if (*g.to(sn) == node) return last_pos_in(sn, g);

	return none;
}

void split_at(Graph & g, PositionInSequence const pis)
{
	if (node(g, pis)) throw runtime_error("cannot split node");

	Sequence a = g[pis.sequence], b = a;
	
	a.positions.erase(a.positions.begin() + pis.position.index + 1, a.positions.end());
	b.positions.erase(b.positions.begin(), b.positions.begin() + pis.position.index);

	g.set(pis.sequence, a);
	g.set(none, b);
}

pair<vector<Position>, ReorientedNode> follow(Graph const & g, ReorientedNode const & n, SeqNum const s, unsigned const frames_per_pos)
{
	vector<Position> positions;
	ReorientedNode m;

	if (*g.from(s) == *n)
	{
		PositionReorientation const r = compose(inverse(g.from(s).reorientation), n.reorientation);

		assert(basicallySame( 
			g[s].positions.front(),
			g[g.from(s)],
			g.from(s).reorientation(g[*g.from(s)].position),
			g.from(s).reorientation(g[*n].position),
			g[first_pos_in(s)] ));

		assert(basicallySame(
			r(g[first_pos_in(s)]),
			r(g.from(s).reorientation(g[*n].position)),
			compose(inverse(g.from(s).reorientation), n.reorientation)(g.from(s).reorientation(g[*n].position)),
			n.reorientation(inverse(g.from(s).reorientation)(g.from(s).reorientation(g[*n].position))),
			n.reorientation(g[*n].position),
			g[n]
			));

		for (PositionInSequence location = first_pos_in(s);
			next(location, g);
			location = *next(location, g))
					// See GCC bug 68003 for the reason behind the DRY violation.

			for (unsigned howfar = 0; howfar <= frames_per_pos; ++howfar)
				positions.push_back(between(
					r(g[location]),
					r(g[*next(location, g)]),
					howfar / double(frames_per_pos)));

		*m = *g.to(s);
		m.reorientation = compose(g.to(s).reorientation, r);

		assert(basicallySame(
			positions.back(),
			r(g[g.to(s)]),
			r(g.to(s).reorientation(g[*g.to(s)].position)),
			r(g.to(s).reorientation(g[*m].position)),
			r(g.to(s).reorientation(g[*m].position)),
			compose(g.to(s).reorientation, r)(g[*m].position),
			m.reorientation(g[*m].position),
			m.reorientation(g[*g.to(s)].position),
			g[m]
			));
	}
	else if (*g.to(s) == *n)
	{
		assert(basicallySame(
			g[s].positions.back(),
			g[g.to(s)],
			g.to(s).reorientation(g[*g.to(s)].position),
			g.to(s).reorientation(g[*n].position),
			g[last_pos_in(s, g)]
			));

		PositionReorientation const r = compose(inverse(g.to(s).reorientation), n.reorientation);

		assert(basicallySame(
			r(g[last_pos_in(s, g)]),
			r(g.to(s).reorientation(g[*n].position)),
			compose(inverse(g.to(s).reorientation), n.reorientation)(g.to(s).reorientation(g[*n].position)),
			n.reorientation(inverse(g.to(s).reorientation)(g.to(s).reorientation(g[*n].position))),
			n.reorientation(g[*n].position),
			g[n]
			));

		for (PositionInSequence location = last_pos_in(s, g);
			prev(location);
			location = *prev(location))
					// See GCC bug 68003 for the reason behind the DRY violation.

			for (unsigned howfar = 0; howfar <= frames_per_pos; ++howfar)
				positions.push_back(between(
					r(g[location]),
					r(g[*prev(location)]),
					howfar / double(frames_per_pos)));

		*m = *g.from(s);
		m.reorientation = compose(g.from(s).reorientation, r);
	}
	else throw std::runtime_error(
		"node " + std::to_string(n->index) + " is not connected to sequence " + std::to_string(s.index));

	assert(basicallySame(positions.front(), g[n]));
	assert(basicallySame(positions.back(), g[m]));

	return {positions, m};
}

ReorientedNode follow(Graph const & g, ReorientedNode const & n, SeqNum const s)
{
	if (*g.from(s) == *n)
		return
			{ *g.to(s)
			, compose(g.to(s).reorientation,
			  compose(inverse(g.from(s).reorientation),
			  n.reorientation)) };
	else if (*g.to(s) == *n)
		return { *g.from(s)
			, compose(g.from(s).reorientation,
			  compose(inverse(g.to(s).reorientation),
			  n.reorientation)) };
	else throw std::runtime_error(
		"node " + std::to_string(n->index) + " is not connected to sequence " + std::to_string(s.index));
}

NodeNum follow(Graph const & g, NodeNum const n, SeqNum const s)
{
	if (*g.from(s) == n) return *g.to(s);
	if (*g.to(s) == n) return *g.from(s);
	throw std::runtime_error(
		"node " + std::to_string(n.index) + " is not connected to sequence " + std::to_string(s.index));
}

set<string> tags_in_desc(vector<string> const & desc)
{
	set<string> r;

	string const decl = "tags:";

	foreach(line : desc)
	{
		if (line.substr(0, decl.size()) == decl)
		{
			std::istringstream iss(line.substr(decl.size()));
			string tag;
			while (iss >> tag) r.insert(tag);
		}
	}

	return r;
}

TagQuery query_for(Graph const & g, NodeNum const n)
{
	TagQuery q;

	foreach (t : tags(g[n]))
		q.insert(make_pair(t, true));

	while (q.size() < 10)
	{
		map<string, int> c;

		foreach (m : match(g, q))
			if (m != n)
				foreach (t : tags(g[m]))
					if (q.find(make_pair(t, true)) == q.end()
						&& q.find(make_pair(t, false)) == q.end())
						++c[t];

		if (c.empty()) break;

		auto me = std::max_element(c.begin(), c.end(),
			[](pair<string, int> const & x, pair<string, int> const & y)
			{
				return x.second < y.second;
			});

		q.insert(make_pair(me->first, false));
	}

	return q;
}

set<string> properties_in_desc(vector<string> const & desc)
{
	set<string> r;

	string const decl = "properties:";

	foreach(line : desc)
	{
		if (line.substr(0, decl.size()) == decl)
		{
			std::istringstream iss(line.substr(decl.size()));
			string prop;
			while (iss >> prop) r.insert(prop);
		}
	}

	return r;
}

set<string> tags(Graph const & g)
{
	set<string> r;

	foreach(n : nodenums(g)) foreach(t : tags(g[n])) r.insert(t);
	foreach(s : seqnums(g)) foreach(t : tags(g[s])) r.insert(t);

	return r;
}

bool connected(Graph const & g, NodeNum const a, set<NodeNum> const & s)
{
	foreach(b : s)
		if (connected(g, a, b)) return true;

	return false;
}

bool connected(Graph const & g, NodeNum const a, NodeNum const b)
{
	foreach (s : g[a].in)
		if (*from(g, s) == b)
			return true;
	
	foreach (s : g[a].out)
		if (*to(g, s) == b)
			return true;

	return false;
}

std::set<NodeNum> grow(Graph const & g, std::set<NodeNum> const & nodes)
{
	auto r = nodes;

	foreach(n : nodenums(g))
		if (connected(g, n, nodes)) { r.insert(n); break; }

	return r;
}

std::set<NodeNum> grow(Graph const & g, std::set<NodeNum> nodes, unsigned const depth)
{
	for (unsigned d = 0; d != depth; ++d) nodes = grow(g, nodes);
	return nodes;
}

set<NodeNum> nodes_around(Graph const & g, set<NodeNum> const & nodes, unsigned depth)
{
	set<NodeNum> all = nodes;
	set<NodeNum> r;

	for (unsigned d = 0; d != depth; ++d)
	{
		set<NodeNum> const prev = all;

		foreach(n : nodenums(g))
			if (connected(g, n, prev) && all.count(n) == 0)
			{
				all.insert(n);
				r.insert(n);
			}
	}

	return r;
}

Reoriented<Reversible<SeqNum>>
	connect_in(Reoriented<NodeNum> const & n, Reversible<SeqNum> const s, Graph const & g)
{
	return Reoriented<Reversible<SeqNum>>
		{s, compose(inverse(to(g, s).reorientation), n.reorientation)};
}

Reoriented<Reversible<SeqNum>>
	connect_out(Reoriented<NodeNum> const & n, Reversible<SeqNum> const s, Graph const & g)
{
	return Reoriented<Reversible<SeqNum>>
		{s, compose(inverse(from(g, s).reorientation), n.reorientation)};
}

vector<Reoriented<SegmentInSequence>> segments_around(ReorientedNode const & n, Graph const & g)
{
	vector<Reoriented<SegmentInSequence>> r;

	foreach (x : in_segments(n, g))
		r.push_back(Reoriented<SegmentInSequence>{**x, x.reorientation});

	foreach (x : out_segments(n, g))
		r.push_back(Reoriented<SegmentInSequence>{**x, x.reorientation});
		
	return r;
}

vector<ReorientedSegment> neighbours(
	ReorientedSegment const & s, Graph const & g,
	bool const open /* include segments in neighbouring sequences */)
{
	vector<ReorientedSegment> n;

	if (s->segment != last_segment(s->sequence, g).segment) // forward
		n.push_back({next(*s), s.reorientation});
	else if (open)
		n += segments_around(to(sequence(s), g), g);

	if (auto x = prev(*s)) // backward
		n.push_back({*x, s.reorientation});
	else if (open)
		n += segments_around(from(sequence(s), g), g);

	return n;
}

}
