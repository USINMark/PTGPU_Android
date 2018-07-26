
#include "../include/KDTree.h"
#include "../include/AccelCommon.h"

Bound KDTree::getBound(Sphere s)
{
	Bound b;

	b.min_x = s.p.s[0] - s.rad;
	b.max_x = s.p.s[0] + s.rad;
	b.min_y = s.p.s[1] - s.rad;
	b.max_y = s.p.s[1] + s.rad;
	b.min_z = s.p.s[2] - s.rad;
	b.max_z = s.p.s[2] + s.rad;

	return b;
}

Bound KDTree::getBound(Triangle t)
{
	Bound b;

	b.min_x = min3(t.p1.s[0], t.p2.s[0], t.p3.s[0]);//[t.p1].p.x, m_pois[t.p2].p.x, m_pois[t.p3].p.x);
	b.max_x = max3(t.p1.s[0], t.p2.s[0], t.p3.s[0]);//m_pois[t.p1].p.x, m_pois[t.p2].p.x, m_pois[t.p3].p.x);
	b.min_y = min3(t.p1.s[1], t.p2.s[1], t.p3.s[1]);//m_pois[t.p1].p.y, m_pois[t.p2].p.y, m_pois[t.p3].p.y);
	b.max_y = max3(t.p1.s[1], t.p2.s[1], t.p3.s[1]);//m_pois[t.p1].p.y, m_pois[t.p2].p.y, m_pois[t.p3].p.y);
	b.min_z = min3(t.p1.s[2], t.p2.s[2], t.p3.s[2]);//m_pois[t.p1].p.z, m_pois[t.p2].p.z, m_pois[t.p3].p.z);
	b.max_z = max3(t.p1.s[2], t.p2.s[2], t.p3.s[2]);//m_pois[t.p1].p.z, m_pois[t.p2].p.z, m_pois[t.p3].p.z);

	return b;
}

Bound KDTree::getBound(Shape s)
{
	Bound b;

	if (s.type == TRIANGLE) b = getBound(s.t);
	else if (s.type == SPHERE) b = getBound(s.s);

	return b;
}

Vec KDTree::getMidpoint(Sphere s)
{
	return s.p;
}

Vec KDTree::getMidpoint(Triangle t)
{
	Vec v;

	v.s[0] = (t.p1.s[0], t.p2.s[0], t.p3.s[0]) / 3.0f;// m_pois[t.p1].p.x + m_pois[t.p2].p.x + m_pois[t.p3].p.x) / 3.0f;
	v.s[1] = (t.p1.s[1], t.p2.s[1], t.p3.s[1]) / 3.0f;// m_pois[t.p1].p.y + m_pois[t.p2].p.y + m_pois[t.p3].p.y) / 3.0f;
	v.s[2] = (t.p1.s[2], t.p2.s[2], t.p3.s[2]) / 3.0f;// m_pois[t.p1].p.z + m_pois[t.p2].p.z + m_pois[t.p3].p.z) / 3.0f;

	return v;
}

Vec KDTree::getMidpoint(Shape s)
{
	Vec v;

	if (s.type == TRIANGLE) v = getMidpoint(s.t);
	else if (s.type == SPHERE) v = getMidpoint(s.s);

	return v;
}

int KDTree::getLongestAxis(Bound b)
{
	float xdist = b.max_x - b.min_x, ydist = b.max_y - b.min_y, zdist = b.max_z - b.min_z;

	if (xdist >= ydist) {
		if (xdist >= zdist) return 0;
		else return 2;
	}
	else {
		if (ydist >= zdist) return 1;
		else return 2;
	}

	return 2;
}

// 삼각형들을 위한 KDTREE 생성 
KDTreeNode* KDTree::build(std::vector<Shape *> s, int depth)
{
	KDTreeNode* node = new KDTreeNode();
	if (depth > m_maxdepth) m_maxdepth = depth;

	node->leaf = false;
    node->shapes = std::vector<Shape *>();
	node->left = NULL;//"
    node->right = NULL;//초기화
	node->box = Bound();

	if (s.size() == 0) return node;
	if (s.size() <= 1) //depth > MAX_KDTREEDEPTH || )
	{
		node->shapes = s;
		node->leaf = true;
		node->box = getBound(*s[0]);

		for (long i = 1; i<s.size(); i++) {
			node->expandBound(getBound(*s[i]));
		}

		node->left = NULL; // new KDNode(m_pois, m_poiCnt);
		node->right = NULL; // new KDNode(m_pois, m_poiCnt);

		//node->left->shapes = std::vector<Shape *>();
		//node->right->shapes = std::vector<Shape*>();

		m_szbuf += s.size();
		return node;
	}

	Vec midpoint;		
	float shapesRecp = 1.0 / s.size();

	midpoint.s[0] = midpoint.s[1] = midpoint.s[2] = 0.0;
	node->box = getBound(*s[0]);

	for (long i = 1; i < s.size(); i++)
	{
		node->expandBound(getBound(*s[i]));

		Vec m = getMidpoint(*s[i]);

		m.s[0] *= shapesRecp, m.s[1] *= shapesRecp, m.s[2] *= shapesRecp;
		midpoint.s[0] += m.s[0], midpoint.s[1] += m.s[1], midpoint.s[2] += m.s[2];
	}
	
	std::vector<Shape *> left_shapes;
	std::vector<Shape *> right_shapes;

	int axis = getLongestAxis(node->box); //xyz 중 가장 긴축 (x:0,y:1,z:2)

	for (long i = 0; i < s.size(); i++)
	{
		switch (axis) {
		case 0://중앙 값보다 크면 오른쪽 트리에, 아니면 왼쪽 트리에 넣는다
			midpoint.s[0] >= getMidpoint(*s[i]).s[0] ? right_shapes.push_back(s[i]) : left_shapes.push_back(s[i]);
			break;
		case 1:
			midpoint.s[1] >= getMidpoint(*s[i]).s[1] ? right_shapes.push_back(s[i]) : left_shapes.push_back(s[i]);
			break;
		case 2:
			midpoint.s[2] >= getMidpoint(*s[i]).s[2] ? right_shapes.push_back(s[i]) : left_shapes.push_back(s[i]);
			break;
		}
	}

	if (s.size() == left_shapes.size() || s.size() == right_shapes.size())
	{
		node->shapes = s;
		node->leaf = true;
		node->box = getBound(*s[0]);

		for (long i = 1; i < s.size(); i++) {
			node->expandBound(getBound(*s[i]));
		}

		node->left = NULL; // new KDNode(m_pois, m_poiCnt);
		node->right = NULL; // new KDNode(m_pois, m_poiCnt);

		//node->left->shapes = std::vector<Shape *>();
		//node->right->shapes = std::vector<Shape *>();

		m_szbuf += (left_shapes.size() + right_shapes.size());
		return node;
	}

	//깊이를 더하여 재귀
	node->left = build(left_shapes, depth + 1);
	node->right = build(right_shapes, depth + 1);
	
    return node;
}

void KDTree::fillNode(KDTreeNode *tnode, int &locnode, int &locshape, KDNodeGPU *kngnode, int *pnbuf)
{
	kngnode[locnode].bound = tnode->box;
	kngnode[locnode].leaf = tnode->leaf;

	if (tnode->leaf)
	{
		kngnode[locnode].min = locshape;

		for (int i = 0; i < tnode->shapes.size(); i++)
			pnbuf[locshape++] = tnode->shapes[i]->index;

		kngnode[locnode].max = locshape;
	}

	//kngnode[locnode].nShape = tnode->shapes.size();

	kngnode[locnode].nLeft = 2 * locnode;
	kngnode[locnode].nRight = 2 * locnode + 1;
	kngnode[locnode].nParent = locnode / 2;
}

void KDTree::traverseTree(KDTreeNode *tnode, int locNode, int locShape, KDNodeGPU *pkngbuf, int *pknbuf)
{
	int curnodeloc = locNode, curshapeloc = locShape, size = pow(2, m_maxdepth + 1);

	std::queue<KDTreeNode *> q;
	q.push(tnode);

	while (!q.empty())
	{
		KDTreeNode *node = q.front();
		q.pop();
				
		if (node)
		{
			fillNode(node, curnodeloc, curshapeloc, pkngbuf, pknbuf);
			curnodeloc++;

			//if (node->leaf) continue;

			if (node->left) q.push(node->left);
			else q.push(NULL);

			if (node->right) q.push(node->right);
			else q.push(NULL);
		}
		else
		{
			q.push(NULL);
			q.push(NULL);

			curnodeloc++;
		}

		if (curnodeloc >= size) break;
	}
}

void KDTree::traverseTreeDFS(KDTreeNode *tnode, int &locNode, int &locShape, KDNodeGPU *pkngbuf, int *pknbuf)
{
	if (tnode)
	{
		int curnode = locNode;
		fillNode(tnode, curnode, locShape, pkngbuf, pknbuf);
				
		traverseTreeDFS(tnode->left, ++curnode, locShape, pkngbuf, pknbuf);
		traverseTreeDFS(tnode->right, ++curnode, locShape, pkngbuf, pknbuf);

		locNode = curnode;
	}
}

void KDTree::getTrees(KDTreeNode *rootNode, KDNodeGPU **ppkngbuf, int *pkngCnt, int **ppknbuf, int *pknCnt)
{
	int *pknbuf = (int *)malloc(sizeof(int) * m_szbuf), size = pow(2, m_maxdepth + 1);
	KDNodeGPU *pkngbuf = (KDNodeGPU *)malloc(sizeof(KDNodeGPU) * size);

	//int locNode = 0, locShape = 0;
	traverseTree(rootNode, 1, 0, pkngbuf, pknbuf);
	//traverseTreeDFS(rootNode, locNode, locShape, pkngbuf, pknbuf);

	*ppkngbuf = pkngbuf;
	*ppknbuf = pknbuf;
	*pkngCnt = size;
	*pknCnt = m_szbuf;
}