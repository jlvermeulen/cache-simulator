#pragma once
include < stdint.h > ;
class PLRUtree
{
public:
	uint32_t size;
	PLRUtree(unsigned size) :size(size){}
	~PLRUtree();
	uint32_t addresses[16];
	bool binaryTree[16];

	uint32_t getOverwriteTarget()
	{
		uint32_t s = size / 2;
		uint32_t t = s;
		while (s!=1)
		{
			s /= 2;
			if (binaryTree[t])
				t += s;
			else
				t -= s;
		}
		if (binaryTree[t])
			return t;
		else
			return t - 1;
	}


	void setPath(uint32_t element)
	{
		uint32_t s = size / 2;
		uint32_t t = s;
		while (s != 1)
		{
			s /= 2;
			binaryTree[t] = element < t;
			if (!binaryTree[t])
				t += s;
			else
				t -= s;
		}

	}

};

