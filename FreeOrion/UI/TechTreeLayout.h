#ifndef _TechTreeLayout_h_
#define _TechTreeLayout_h_
#include <map>
#include <list>
#include <string>
#include <vector>

#include <GG/PtRect.h>
#include "../universe/Tech.h"


/**
 * This class stores internal information for layouting the tech graph.
 * The whole space is organized in a table.
 * Every tech gets it's own tech level as maximum distance from start tech.
 * Every tech level (m_depth) is one column.
 * The techs are sorted by depth->category->parent count->name
 * The first column is filled with every level 1 tech with a gap between each category. 
 * For every following column the nodes check their parents and try to place them near the average of their parents.
 * For children that don't lie in the next column we 
 * generate dummy techs that are rendered as horizontal line (part of the future connection arrow) that connect
 * the low tech with the high tech. This allows complexe connection lines.
 * Dummy techs are directly processed in order to keep them as straight as possible.
 * 
 *
 * sample (without category)
 * A->DE B->D C->F D->F E->F
 * A(1) D(2) E(2) B(1) D(2) C(1) F(3)
 * C(1)->F(3) changed to C(1)->G*(2)->F(3) (*Dummy)
 * 1. Column We sort the start Techs
 *    A   |     | 
 *    B   |     | 
 *    C   |     | 
 *   C has a Dummy child, which is placed at once
 *    A   |     | 
 *    B   |     | 
 *    C   |  G* | 
 * 2. Column
 *   D has parents A, B. The average is 1 a free place
 *    A   |  D  | 
 *    B   |     | 
 *    C   |  G* | 
 *   E has parents A. The average is 1 but already used, it's place in 2
 *    A   |  D  | 
 *    B   |  E  | 
 *    C   |  G* | 
 * 3. Column
 *   F has parents G*, D, E note that C was replaced with a dummy. The average is 2 a free place
 *    A   |  D  | 
 *    B   |  E  | F
 *    C   |  G* | 
 *
 * now we create the arrows
 *    A-\-/->D--\
 *    B-/ \->E--->F
 *    C -----G*-/ 
 * 
 * author Lathanda
 */
class TechTreeLayout {
public:
    class Node;
    class Edge;
    class Column;
    TechTreeLayout( );
    ~TechTreeLayout( );
    void AddNode(const Tech *tech, GG::X width, GG::Y height);
    const Node* GetNode(const std::string & name) const;
    void AddEdge(const std::string & parent, const std::string & child);
    const GG::X GetWidth( ) const;
    const GG::Y GetHeight( ) const;
    double GetWidth(const std::string & name ) const;
    double GetHeight(const std::string & name ) const;
    void Clear();
    void DoLayout( double column_width, double row_height, double x_margin);
    const std::vector<Edge*> & GetOutEdges(const std::string & name) const;
    void Debug() const;
private:
    double m_width; //width of the complete graph
    double m_height; //height of the complete graph
    int m_row_count;
    int m_column_count;
    std::map< std::string, Node*> m_node_map; //map name->node for external access by name
    std::vector<Node*> m_nodes; // list of nodes for sorting
};

class TechTreeLayout::Edge {
public:
    Edge(const Tech* from, const Tech* to);
    ~Edge( );
    const Tech* GetTechFrom( ) const;
    const Tech* GetTechTo( ) const;
    void ReadPoints(std::vector<std::pair<double,double> > & points) const;
    void AddPoint(double x, double y);
    void Debug( ) const;
private:
    std::vector<std::pair<double,double> > m_points; // point list of connection
    const Tech* m_from; //source tech
    const Tech* m_to;   //destination tech
};

class TechTreeLayout::Node {
    friend class TechTreeLayout;
    friend class Column;
public:
    Node(const Tech *tech, GG::X width, GG::Y height);
    ~Node( );
    const GG::X GetX() const;
    const GG::Y GetY() const;
    unsigned int GetDepth() const;
    const Tech *GetTech( ) const;
    const std::string& GetName() const;
    const std::vector<Edge*>& GetOutEdges( ) const;
    int GetNumberOfChildren( ) const;
    int GetNumberOfParents( ) const;
    void Debug() const;
    bool IsFinalNode( ) const;
    bool IsStartNode( ) const;
    bool operator < (const Node& y) const;
    const int m_weight;            // height in rows
private:
    Node( Node *parent, Node *child, std::vector<Node*> & nodes);
    bool Wobble(Column & column);
    bool IsPlaceHolder( ) const;
    void AddChild(Node* node);
    void CalculateDepth( );
    double CalculateFamilyDistance(int row );
    void CreatePlaceHolder( std::vector<Node*> & nodes);
    void DoLayout( std::vector<Column> & row_index, bool cat );
    void CalculateCoordinate(double column_width, double row_height);
    void CreateEdges( double x_margin, double column_width, double row_height );
    void CalculateDepth( int depth );
    int m_depth; // depth 1 available at beginning 2 one requisite etc
    int m_row;   //layout row, every node is organized in a straight tabelle system
    const Tech* m_tech;     // associated tech
    std::string m_name;     // name
    double m_x;              // left border
    double m_y;              // top border
    double m_width;          // width
    double m_height;         // height
    bool   m_place_holder;   // is place holder
    int    m_children_rows;  // height in cells for layouting
    std::vector<Node*> m_parents;  // parents
    std::vector<Node*> m_children; // children
    Node* m_child ;              // primary child for layout
    std::vector<Edge*> m_out_edges;// outgoing edges
};
class TechTreeLayout::Column {
public:
    Column();
    bool Fit(int index, TechTreeLayout::Node* node);
    bool PlaceNextFree(int index, TechTreeLayout::Node* node);
    int NextFree(int index, TechTreeLayout::Node* node);
    bool Place(int index, TechTreeLayout::Node* node);
    bool Move(int to, TechTreeLayout::Node* node);
    Node* Seek(Node* m, int direction);
    bool Swap(Node* m, Node* n);
    unsigned int Size();
private:
    std::vector<TechTreeLayout::Node*> m_column;
};
#endif