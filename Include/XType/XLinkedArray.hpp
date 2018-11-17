#pragma once

template <typename T>
class XLinkedArray
{
private:
	size_t _size;
	linked_list_head_p _head;

public:
	XLinkedArray();
	~XLinkedArray();

	T * Append();

	void Iterate(void(*)(T * obj));

	error_t FreeEntry(T * entry);

	size_t Length();
	size_t DataSize();
};