"""Keep boundary value tags explicit across the target and development C++ libraries."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class BoundaryValueConstructionTests(unittest.TestCase):
    def test_literal_pointer_and_existing_tags(self):
        root = Path(__file__).resolve().parents[2]
        source = r'''
#include "resident/checkpoint/ordered_value.hpp"
#include <iostream>
using namespace hanmo::resident;
int main() {
    const char* pointer = "fixed_actor_appearance_composition_v1";
    char mutable_text[] = "mutable";
    BoundaryValue literal{"fixed_actor_appearance_composition_v1"};
    BoundaryValue indirect{pointer};
    BoundaryValue mutable_string{mutable_text};
    BoundaryValue explicit_string{std::string{"text"}};
    BoundaryValue empty;
    BoundaryValue boolean{true};
    BoundaryValue integer{Integer{42}};
    BoundaryValue zero{Integer{0}};
    BoundaryValue tagged{BoundaryValue::Data{Integer{0}}};
    BoundaryValue real{1.5};
    BoundaryValue list{BoundaryValue::Array{literal, boolean, integer}};
    BoundaryValue dictionary{BoundaryValue::Dict{{"kind", literal}}};
    BoundaryValue copied = literal;
    BoundaryValue moved = std::move(copied);
    if (!literal.as<std::string>() || *literal.as<std::string>() != pointer ||
        !indirect.as<std::string>() || !mutable_string.as<std::string>() ||
        !explicit_string.as<std::string>() || !moved.as<std::string>() ||
        !empty.as<std::monostate>() || !boolean.as<bool>() ||
        !integer.as<Integer>() || !zero.as<Integer>() || *zero.as<Integer>() != 0 ||
        !tagged.as<Integer>() || !real.as<double>() ||
        !list.as<BoundaryValue::Array>() || !dictionary.as<BoundaryValue::Dict>()) return 1;
    if (literal.data.index() != 4 || boolean.data.index() != 1 ||
        integer.data.index() != 2 || real.data.index() != 3 ||
        list.data.index() != 5 || dictionary.data.index() != 6) return 2;
    std::cout << "BOUNDARY_VALUE_TAGS_PASS literal=string pointer=string compiler=" << __VERSION__ << '\n';
}
'''
        with tempfile.TemporaryDirectory(prefix='hanmo-boundary-') as temporary:
            folder = Path(temporary)
            cpp, executable = folder / 'tags.cpp', folder / 'tags'
            cpp.write_text(source)
            subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17',
                            '-I' + str(root / 'native/world/include'),
                            '-I' + str(root / 'native/world/src'),
                            str(cpp), '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
