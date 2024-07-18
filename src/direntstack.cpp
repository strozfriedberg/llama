#include "direntstack.h"
#include "hex.h"
#include "recordhasher.h"

bool DirentStack::empty() const {
  return Stack.empty();
}

const Dirent& DirentStack::top() const {
  return Stack.top().Rec;
}

Dirent DirentStack::pop() {
  // pop the record and trim back the path
  Element& e = Stack.top();
  Path.resize(e.LastPathSepIndex);
  Dirent rec{std::move(e.Rec)};
  Stack.pop();

  // hash the record
  const FieldHash fhash{RecHasher.hashDirent(rec)};
  std::string hash = hexEncode(&fhash.hash, sizeof(fhash.hash));

  // add the hash to the parent, if any
//  if (!Stack.empty()) {
//    Stack.top().Record["children"].push_back(hash);
//  }

  // put the hash into the record
  rec.Id = std::move(hash);

  return rec;
}

void DirentStack::push(Dirent&& rec) {
  const size_t len = Path.length();
  if (len > 0) {
    Path.append("/");
  }
  Path.append(rec.Name);

  rec.Path = Path;

  Stack.push({std::move(rec), len});
}
